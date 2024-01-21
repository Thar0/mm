#include "z64sample.h"
#include "aiff.h"
#include "vadpcm.h"

///////////////////////////////////////////////////////////////////////////////

static void read_COMM(z64sample* this, void* p) {
	AiffComm* comm = readbuf(AiffComm, p);

	if ((this->chan_num = comm->chan_num) >= SAMPLE_CHANNEL_MAX)
		throw(ERR_ERROR, "channel num %d / %d max", this->chan_num, SAMPLE_CHANNEL_MAX - 1);
	this->sample_num =
		wmask(comm->sample_num_h, 0xFFFF0000)
		| wmask(comm->sample_num_l, 0x0000FFFF);
	this->bit_depth = comm->bit_depth;

	float80* rate = offsetptr(*comm, sample_ratef80);
	membswap(rate, 10);

	this->sample_rate = *rate;
	this->endian = SAMPLE_ENDIAN_BIG;

	if (comm->chunk_size + sizeof(AiffChunk) > sizeof(AiffComm)) {
		char* name = (void*)readbuf(uint32_t, p);
		static int8_t type[512] = {
			[cchash('N', 'O', 'N', 'E') % 512] = SAMPLE_TYPE_INT + 1,
			[cchash('f', 'l', '3', '2') % 512] = SAMPLE_TYPE_FLOAT + 1,
			[cchash('f', 'l', '6', '4') % 512] = SAMPLE_TYPE_FLOAT + 1,
			[cchash('F', 'L', '3', '2') % 512] = SAMPLE_TYPE_FLOAT + 1,
			[cchash('F', 'L', '6', '4') % 512] = SAMPLE_TYPE_FLOAT + 1,

			[cchash('V', 'A', 'P', 'C') % 512] = SAMPLE_TYPE_VAD9 + 1,
			[cchash('A', 'D', 'P', '9') % 512] = SAMPLE_TYPE_VAD9 + 1,
			[cchash('A', 'D', 'P', '5') % 512] = SAMPLE_TYPE_VAD5 + 1,
		};

		this->type = type[hashmem(name, 4) % 512] - 1;
		memcpy(this->compression, name, 4);

		if (this->type < 0)
			throw(ERR_ERROR, "unsupported compression type: %.4s", name);
	} else
		this->type = SAMPLE_TYPE_INT;
}

static void read_MARK(z64sample* this, void* p) {
	AiffMark* mark = p;

	if (!(this->num_mark = mark->num))
		return;

	AiffMarkEntry* entry = mark->entry;
	int index_cap = 0;

	for (int i = 0; i < this->num_mark; i++, entry++) {
		if (index_cap < entry->index + 1) {
			renew(this->mark, uint32_t[index_cap], uint32_t[entry->index + 1]);
			index_cap = entry->index + 1;
		}

		this->mark[entry->index] =
			wmask(entry->position_h, 0xFFFF0000)
			| wmask(entry->position_l, 0x0000FFFF);

		if (entry->name_len)
			entry = (void*)((char*)entry + (entry->name_len));
	}
}

static void read_INST(z64sample* this, void* p) {
	AiffInst* inst = p;

	this->base_note = inst->base_note;
	this->fine_tune = inst->fine_tune;
	this->key_low = inst->key_low;
	this->key_hi = inst->key_hi;
	this->vel_low = inst->vel_low;
	this->vel_hi = inst->vel_hi;

	if (inst->loop[0].type && this->mark) {
		this->loop_start =  this->mark[inst->loop[0].start];
		this->loop_end = this->mark[inst->loop[0].end];
	}
}

static void read_SSND(z64sample* this, void* p) {
	AiffSsnd* ssnd = p;
	int sample_size = this->bit_depth / 8;

	if (!g_read_data_func_tbl[this->type][this->endian][this->chan_num])
		throw(ERR_ERROR, "%d channels are not supported", this->chan_num);
	if (!(this->samples_l = new(char[sample_size * this->sample_num])))
		throw(ERR_ALLOC, "this->samples_l");
	if (this->chan_num == 2)
		if (!(this->samples_r = new(char[sample_size * this->sample_num])))
			throw(ERR_ALLOC, "this->samples_r");

	g_read_data_func_tbl[this->type][this->endian][this->chan_num](this, ssnd->buf, sample_size);
}

static void read_APPL(z64sample* this, void* p) {
	p += sizeof(AiffChunk);

	if (memeq(p, "stoc\x0BVADPCMCODES", 0x10))
		z64sample_read_codebook(this, p + 0x12);

	if (memeq(p, "stoc\x0BVADPCMLOOPS", 0x10)) {
		p += 0x14;
		this->loop_start = swapBE(*readbuf(uint32_t, p));
		this->loop_end = swapBE(*readbuf(uint32_t, p));
		this->loop_num = swapBE(*readbuf(uint32_t, p));
		memcpy(this->codebook_ctx->loop_state, p, sizeof(int16_t[16]));
		this->codebook_ctx->loop_seek = SIZE_MAX;
	}
}

static void read_ZZFM(z64sample* this, void* p) {
	p += sizeof(AiffChunk);

	memcpy(this->compression, p, 4);
}

int z64sample_impl_read_aiff(z64sample* this, const char* name) {
	File __f = {}, * f = &__f;

	try {
		if (!sys_stat(name))
			throw(ERR_ERROR, "could not find file: %s", name);

		if (f_open(f, name, .bin = true))
			throw(ERR_ERROR, "could not open file: %s", name);

		uint8_t* data = f->v_data;
		AiffChunk* header = readbuf(AiffChunk, data);
		uint32_t* aiff = readbuf(uint32_t, data);

		if (!memeq(header->chunk_name, "FORM", 4))
			throw(ERR_ERROR, "not a valid FORM file!");
		if (!memeq(aiff, "AIFF", 4) && !memeq(aiff, "AIFC", 4))
			throw(ERR_ERROR, "not a valid AIFF file!");

		uint8_t* end = data + header->chunk_size;

		for (; data < end;) {
			#define TBL_MOD 512
			AiffChunk* chunk = readbuf(AiffChunk, data);
			uint32_t hash = hashmem(chunk->chunk_name, 4) % TBL_MOD;
			z64ChunkReadFunc chunk_func_tbl[TBL_MOD] = {
				// hash table
				[cchash('C', 'O', 'M', 'M') % TBL_MOD] = read_COMM,
				[cchash('M', 'A', 'R', 'K') % TBL_MOD] = read_MARK,
				[cchash('I', 'N', 'S', 'T') % TBL_MOD] = read_INST,
				[cchash('S', 'S', 'N', 'D') % TBL_MOD] = read_SSND,
				[cchash('A', 'P', 'P', 'L') % TBL_MOD] = read_APPL,
				[cchash('Z', 'Z', 'F', 'M') % TBL_MOD] = read_ZZFM,
			};

			if (hash == cchash('\0', '\0', '\0', '\0') || !chunk->chunk_size)
				break;

			print_align(
				"chunk:", COL_BLUE "%.4s :: %04X" COL_RESET "%21s" COL_RESET,
				chunk->chunk_name, hash % TBL_MOD,
				x_fmt("" COL_GRAY "0x" COL_BLUE "%X", chunk->chunk_size));

			if (chunk_func_tbl[hash])
				chunk_func_tbl[hash](this, (void*)chunk);

			data += alignval(chunk->chunk_size, 2);
		}

		z64sample_print_state(this);
		print_align("samples:",    "%d", this->sample_num);
		print_align("samplerate:", "%d", this->sample_rate);

	} except {
		f_destroy(f);
		print_exception();
		return EXIT_FAILURE;
	}

	f_destroy(f);

	return EXIT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////

#define ALIGN_CHUNK 2
#define SIZE_CHUNK  sizeof(AiffChunk)
#define DEF_CHUNK(var, name) \
		do { \
			uint32_t __chunk_size_offset = f->seek + __builtin_offsetof(AiffChunk, chunk_size); \
			uint32_t __chunk_size = f->size + SIZE_CHUNK; \
			memcpy(var.chunk_name, name, 4)
#define WRITE_TO_CHUNK(var)          f_write(f, &var, sizeof(var)); f_align(f, ALIGN_CHUNK)
#define WRITE_TO_CHUNK_NO_ALIGN(var) f_write(f, &var, sizeof(var))
#define END_CHUNK() \
		f_align(f, ALIGN_CHUNK); \
		f_seek(f, __chunk_size_offset); \
		f_writeBE(f, wrval(uint32_t, f->size - __chunk_size)); \
		f_seek(f, FILE_END); \
		break; \
		} while (0)

static void write_COMM(z64sample* this, File* f) {
	AiffComm comm = {};

	DEF_CHUNK(comm, "COMM");

	comm.chan_num = this->chan_num;

	comm.sample_num_h = rmask(this->sample_num, 0xFFFF0000);
	comm.sample_num_l = rmask(this->sample_num, 0x0000FFFF);
	comm.bit_depth = this->bit_depth;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
	float80* rate = offsetptr(comm, sample_ratef80);
	*rate = this->sample_rate;
	membswap(rate, 10);
#pragma GCC diagnostic pop

	WRITE_TO_CHUNK(comm);

	switch (this->type) {
		case SAMPLE_TYPE_INT:
			break;

		case SAMPLE_TYPE_FLOAT:
			if (this->bit_depth == 32)
				f_write(f, "fl32", 4);
			else
				f_write(f, "fl64", 4);
			break;

		case SAMPLE_TYPE_VAD9:
			f_write(f, "ADP9", 4);
			f_write(f, "!Nintendo/SGI VADPCM 9-bytes/frame", 0x22);
			break;

		case SAMPLE_TYPE_VAD5:
			f_write(f, "ADP5", 4);
			f_write(f, "!Nintendo/SGI VADPCM 5-bytes/frame", 0x22);
			break;

		default:
			throw(ERR_NONIMPLEMENTED, "sample_type: %s", g_type_name_tbl[this->type % SAMPLE_TYPE_MAX]);
			break;
	}

	END_CHUNK();
}

static void write_MARK(z64sample* this, File* f) {
	AiffMark mark = {};

	if (!this->loop_end || (this->type == SAMPLE_TYPE_VAD9 || this->type == SAMPLE_TYPE_VAD5))
		return;

	DEF_CHUNK(mark, "MARK");

	mark.num = 2;
	this->num_mark = 0xFF;

	AiffMarkEntry start = {
		.index      = 1,
		.position_h = rmask(this->loop_start,0xFFFF0000),
		.position_l = rmask(this->loop_start,0x0000FFFF),
		.name_len   = strlen("start"),
	};
	AiffMarkEntry end = {
		.index      = 2,
		.position_h = rmask(this->loop_end,0xFFFF0000),
		.position_l = rmask(this->loop_end,0x0000FFFF),
		.name_len   = strlen("end"),
	};

	WRITE_TO_CHUNK_NO_ALIGN(mark);
	WRITE_TO_CHUNK_NO_ALIGN(start);
	f_write(f, "start", start.name_len);
	WRITE_TO_CHUNK_NO_ALIGN(end);
	f_write(f, "end", end.name_len);

	END_CHUNK();
}

static void write_INST(z64sample* this, File* f) {
	AiffInst inst = {};

	DEF_CHUNK(inst, "INST");

	inst.base_note = this->base_note;
	inst.fine_tune = this->fine_tune;
	inst.key_low = this->key_low;
	inst.key_hi = this->key_hi;
	inst.vel_low = this->vel_low;
	inst.vel_hi = this->vel_hi;

	if (this->loop_end && this->num_mark == 0xFF) {
		inst.loop[0].type = 1;
		inst.loop[0].start = 1;
		inst.loop[0].end = 2;
	}

	WRITE_TO_CHUNK(inst);
	END_CHUNK();
}

static void write_APPL(z64sample* this, File* f) {
	AiffChunk chunk = {};

	if (!this->codebook_ctx)
		return;

	DEF_CHUNK(chunk, "APPL");
	WRITE_TO_CHUNK(chunk);
	f_write(f, "stoc\x0BVADPCMCODES", strlen("stoc\x0BVADPCMCODES"));
	f_writeBE(f, wrval(uint16_t, 1));
	f_writeBE(f, wrval(uint16_t, this->codebook_ctx->order));
	f_writeBE(f, wrval(uint16_t, this->codebook_ctx->npred));
	f_write(f, this->codebook_ctx->raw, this->codebook_ctx->order * this->codebook_ctx->npred * 8 * sizeof(int16_t));
	END_CHUNK();

	if (!this->loop_end)
		return;

	DEF_CHUNK(chunk, "APPL");
	struct DTA_BE {
		uint32_t magic;
		uint32_t start;
		uint32_t end;
		uint32_t count;
	} loop = {
		0x00010001,
		this->loop_start,
		this->loop_end,
		this->loop_num,
	};

	WRITE_TO_CHUNK(chunk);
	f_write(f, "stoc\x0BVADPCMLOOPS", strlen("stoc\x0BVADPCMLOOPS"));
	f_write(f, &loop, sizeof(loop));
	if (this->codebook_ctx->loop_seek != SIZE_MAX)
		this->codebook_ctx->loop_seek = f->seek;
	f_write(f, this->codebook_ctx->loop_state, sizeof(int16_t[16]));
	END_CHUNK();
}

static void write_SSND(z64sample* this, File* f) {
	AiffSsnd ssnd = {};
	int sample_size = this->bit_depth / 8;

	DEF_CHUNK(ssnd, "SSND");
	WRITE_TO_CHUNK(ssnd);

	if (!g_write_data_func_tbl[this->type][this->endian][this->chan_num])
		throw(ERR_ERROR, "%d channels are not supported", this->chan_num);

	g_write_data_func_tbl[this->type][this->endian][this->chan_num](this, f, sample_size);

	END_CHUNK();
}

static void write_ZZFM(z64sample* this, File* f) {

	if (!this->compression[0])
		return;
	if (this->type == SAMPLE_TYPE_VAD5 || this->type == SAMPLE_TYPE_VAD9)
		return;

	AiffChunk chunk = {};
	DEF_CHUNK(chunk, "ZZFM");
	WRITE_TO_CHUNK(chunk);
	f_write(f, this->compression, 4);
	END_CHUNK();
}

int z64sample_impl_write_aiff(z64sample* this, const char* name) {
	File __f = {}, * f = &__f;

	try {
		this->endian = SAMPLE_ENDIAN_BIG;

		if (this->type == SAMPLE_TYPE_VAD9 || this->type == SAMPLE_TYPE_VAD5) {
			if (!this->codebook_ctx) {
				print_align("table-design:", "generating codebook");
				z64sample_tabledesign(this, this->tabledesign);
			}
		}

		f_write(f, "FORM\0\0\0\0", 8);

		if (strend(name, ".aifc"))
			f_write(f, "AIFC", 4);
		else
			f_write(f, "AIFF", 4);

		write_COMM(this, f);
		write_MARK(this, f);
		write_INST(this, f);
		write_APPL(this, f);
		write_ZZFM(this, f);
		write_SSND(this, f);

		f_seek(f, 4);
		f_writeBE(f, wrval(uint32_t, f->size - sizeof(AiffChunk)));
		z64sample_print_state(this);

		if (f_save(f, name))
			throw(ERR_SILENT);
	} except {
		f_destroy(f);

		print_exception();
		return EXIT_FAILURE;
	}

	f_destroy(f);

	return EXIT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////

int z64sample_impl_read_vadpcm(z64sample* this, const char* name) {
	return z64sample_impl_read_aiff(this, name);
}

int z64sample_impl_write_vadpcm(z64sample* this, const char* name) {
	if (this->bit_depth != SAMPLE_BIT_16 || this->type != SAMPLE_TYPE_INT)
		if (z64sample_set_type_and_bit(this, SAMPLE_TYPE_INT, SAMPLE_BIT_16))
			throw(ERR_ERROR, "error changing channel num!");

	uint32_t tbl[128] = {
		[cchash('V', 'A', 'P', 'C') % 128] = SAMPLE_TYPE_VAD9,
		[cchash('A', 'D', 'P', '9') % 128] = SAMPLE_TYPE_VAD9,
		[cchash('A', 'D', 'P', '5') % 128] = SAMPLE_TYPE_VAD5,
	};
	uint32_t hash = hashmem(this->compression, 4) % 128;

	if (this->output_type == SAMPLE_TYPE_VAD9 || this->output_type == SAMPLE_TYPE_VAD5)
		this->type = this->output_type;
	else if (tbl[hash])
		this->type = tbl[hash];
	else
		this->type = SAMPLE_TYPE_VAD9;
	return z64sample_impl_write_aiff(this, name);
}
