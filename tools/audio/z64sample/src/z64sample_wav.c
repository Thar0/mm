#include "z64sample.h"
#include "wave.h"
#include "vadpcm.h"

///////////////////////////////////////////////////////////////////////////////

static void read_fmt(z64sample* this, void* p) {
	WaveFmt* fmt = readbuf(WaveFmt, p);

	if ((this->chan_num = fmt->chan_num) >= SAMPLE_CHANNEL_MAX)
		throw(ERR_ERROR, "channel num %d / %d max", this->chan_num, SAMPLE_CHANNEL_MAX - 1);
	this->sample_rate = fmt->sample_rate;
	this->bit_depth = fmt->bit_depth;

	this->type = fmt->type == WAVE_TYPE_FLOAT ? SAMPLE_TYPE_FLOAT : SAMPLE_TYPE_INT;
}

static void read_fact(z64sample* this, void* p) {
	WaveFact* fact = readbuf(WaveFact, p);

	this->sample_num = fact->sample_num;
}

static void read_data(z64sample* this, void* p) {
	WaveData* data = (void*)p;
	int frame_size = this->bit_depth / 8;

	if (!this->sample_num)
		this->sample_num = data->chunk_size / frame_size / this->chan_num;

	if (!g_read_data_func_tbl[this->type][this->endian][this->chan_num])
		throw(ERR_ERROR, "%d channels are not supported", this->chan_num);
	if (!(this->samples_l = new(char[frame_size * this->sample_num])))
		throw(ERR_ALLOC, "this->samples_l");
	if (this->chan_num == 2)
		if (!(this->samples_r = new(char[frame_size * this->sample_num])))
			throw(ERR_ALLOC, "this->samples_r");

	g_read_data_func_tbl[this->type][this->endian][this->chan_num](this, data->buf, frame_size);
}

static void read_smpl(z64sample* this, void* p) {
	WaveSmpl* smpl = readbuf(WaveSmpl, p);

	if (smpl->num_sample_loops) {
		WaveSampleLoop* loop = readbuf(WaveSampleLoop, p);

		if (!this->loop_start && !this->loop_end) {
			this->loop_start = loop->start;
			this->loop_end = loop->end;
			this->loop_num = loop->num;
		}
	}
}

static void read_inst(z64sample* this, void* p) {
	WaveInst* inst = (void*)p;

	this->fine_tune = inst->fine_tune;
	this->base_note = inst->base_note;
	this->key_hi = inst->key_hi;
	this->key_low = inst->key_low;
	this->vel_hi = inst->vel_hi;
	this->vel_low = inst->vel_low;
}

static void read_zzbk(z64sample* this, void* p) {
	p += sizeof(WaveChunk);

	z64sample_read_codebook(this, p);
}

static void read_zzlp(z64sample* this, void* p) {
	p += sizeof(WaveChunk);
	(void)p;

	if (!this->codebook_ctx)
		throw(ERR_ERROR, "found zzlp but there's no codebook available!");

	this->loop_start = *readbuf(uint32_t, p);
	this->loop_end = *readbuf(uint32_t, p);
	this->loop_num = *readbuf(uint32_t, p);
	memcpy(this->codebook_ctx->loop_state, p, 32);
	this->codebook_ctx->loop_seek = SIZE_MAX;
}

static void read_zzfm(z64sample* this, void* p) {
	p += sizeof(WaveChunk);

	memcpy(this->compression, p, 4);
}

int z64sample_impl_read_wav(z64sample* this, const char* name) {
	File __f = {}, * f = &__f;

	try {
		this->endian = SAMPLE_ENDIAN_LIL;
		if (!sys_stat(name))
			throw(ERR_ERROR, "could not find file: %s", name);

		if (f_open(f, name, .bin = true))
			throw(ERR_ERROR, "could not open file: %s", name);

		uint8_t* data = f->v_data;
		WaveChunk* header = readbuf(WaveChunk, data);
		uint32_t* wave = readbuf(uint32_t, data);

		if (!memeq(header->chunk_name, "RIFF", 4))
			throw(ERR_ERROR, "not a valid riff file!");
		if (!memeq(wave, "WAVE", 4))
			throw(ERR_ERROR, "not a valid wave file!");

		uint8_t* end = data + header->chunk_size;

		for (; data < end;) {
			#define TBL_MOD 512
			WaveChunk* chunk = readbuf(WaveChunk, data);
			uint32_t hash = hashmem(chunk->chunk_name, 4) % TBL_MOD;
			z64ChunkReadFunc chunk_func_tbl[TBL_MOD] = {
				// hash table
				[cchash('f', 'm', 't', ' ') % TBL_MOD] = read_fmt,
				[cchash('f', 'a', 'c', 't') % TBL_MOD] = read_fact,
				[cchash('d', 'a', 't', 'a') % TBL_MOD] = read_data,
				[cchash('s', 'm', 'p', 'l') % TBL_MOD] = read_smpl,
				[cchash('i', 'n', 's', 't') % TBL_MOD] = read_inst,
				[cchash('z', 'z', 'b', 'k') % TBL_MOD] = read_zzbk,
				[cchash('z', 'z', 'l', 'p') % TBL_MOD] = read_zzlp,
				[cchash('z', 'z', 'f', 'm') % TBL_MOD] = read_zzfm,
			};

			if (hash == cchash('\0', '\0', '\0', '\0') || !chunk->chunk_size)
				break;

			print_align(
				"chunk:", COL_BLUE "%.4s :: %04X" COL_RESET "%21s" COL_RESET,
				chunk->chunk_name, hash % TBL_MOD,
				x_fmt("" COL_GRAY "0x" COL_BLUE "%X", chunk->chunk_size));

			if (chunk_func_tbl[hash])
				chunk_func_tbl[hash](this, (void*)chunk);

			data += alignval(chunk->chunk_size, 4);
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

#define ALIGN_CHUNK 4
#define SIZE_CHUNK  sizeof(WaveChunk)
#define DEF_CHUNK(var, name) \
		do { \
			uint32_t __chunk_size_offset = f->seek + __builtin_offsetof(WaveChunk, chunk_size); \
			uint32_t __chunk_size = f->size + SIZE_CHUNK; \
			memcpy(var.chunk_name, name, 4)
#define WRITE_TO_CHUNK(var)          f_write(f, &var, sizeof(var)); f_align(f, ALIGN_CHUNK)
#define WRITE_TO_CHUNK_NO_ALIGN(var) f_write(f, &var, sizeof(var))
#define END_CHUNK() \
		f_align(f, ALIGN_CHUNK); \
		f_seek(f, __chunk_size_offset); \
		f_write(f, wrval(uint32_t, f->size - __chunk_size)); \
		f_seek(f, FILE_END); \
		break; \
		} while (0)

static void write_fmt(z64sample* this, File* f) {
	WaveFmt fmt = {};
	int frame_size = this->bit_depth / 8;

	DEF_CHUNK(fmt, "fmt ");

	fmt.type = this->type == SAMPLE_TYPE_FLOAT ? WAVE_TYPE_FLOAT : WAVE_TYPE_PCM;
	fmt.chan_num = this->chan_num;
	fmt.sample_rate = this->sample_rate;
	fmt.byte_rate = this->sample_rate * this->chan_num * frame_size;
	fmt.block_align = this->chan_num * frame_size;
	fmt.bit_depth = this->bit_depth;

	WRITE_TO_CHUNK(fmt);
	END_CHUNK();
}

static void write_fact(z64sample* this, File* f) {
	WaveFact fact = {};

	DEF_CHUNK(fact, "fact");
	fact.sample_num = this->sample_num;
	WRITE_TO_CHUNK(fact);
	END_CHUNK();
}

static void write_data(z64sample* this, File* f) {
	WaveData data = {};
	int frame_size = this->bit_depth / 8;

	DEF_CHUNK(data, "data");
	WRITE_TO_CHUNK(data);

	if (!g_write_data_func_tbl[this->type][this->endian][this->chan_num])
		throw(ERR_ERROR, "%d channels are not supported", this->chan_num);

	g_write_data_func_tbl[this->type][this->endian][this->chan_num](this, f, frame_size);

	END_CHUNK();
}

static void write_smpl(z64sample* this, File* f) {
	WaveSmpl smpl = {};

	DEF_CHUNK(smpl, "smpl");

	memcpy(&smpl.manufacturer, "z64audio", 8);
	smpl.num_sample_loops = this->loop_end ? 1 : 0;

	WRITE_TO_CHUNK(smpl);

	if (smpl.num_sample_loops) {
		WaveSampleLoop loop = {
			.cue_point_index = 0x020000,
			.start           = this->loop_start,
			.end             = this->loop_end,
			.num             = this->loop_num
		};

		WRITE_TO_CHUNK(loop);
	}

	END_CHUNK();
}

static void write_inst(z64sample* this, File* f) {
	WaveInst inst = {};

	DEF_CHUNK(inst, "inst");
	inst.base_note = this->base_note;
	inst.fine_tune = this->fine_tune;
	inst.key_low = this->key_low;
	inst.key_hi = this->key_hi;
	inst.vel_low = this->vel_low;
	inst.vel_hi = this->vel_hi;
	WRITE_TO_CHUNK(inst);
	END_CHUNK();
}

static void write_zzbk(z64sample* this, File* f) {
	WaveChunk chunk = {};

	if (!this->codebook_ctx)
		return;

	DEF_CHUNK(chunk, "zzbk");
	WRITE_TO_CHUNK(chunk);
	f_writeBE(f, wrval(uint16_t, this->codebook_ctx->order));
	f_writeBE(f, wrval(uint16_t, this->codebook_ctx->npred));
	f_write(f, this->codebook_ctx->raw, this->codebook_ctx->order * this->codebook_ctx->npred * 8 * sizeof(int16_t));
	END_CHUNK();
}

static void write_zzlp(z64sample* this, File* f) {
	WaveChunk chunk = {};

	if (!this->codebook_ctx)
		return;

	DEF_CHUNK(chunk, "zzlp");
	WRITE_TO_CHUNK(chunk);
	f_write(f, wrval(uint32_t, this->loop_start));
	f_write(f, wrval(uint32_t, this->loop_end));
	f_write(f, wrval(uint32_t, this->loop_num));
	f_write(f, this->codebook_ctx->loop_state, sizeof(uint16_t[16]));
	END_CHUNK();
}

static void write_zzfm(z64sample* this, File* f) {
	WaveChunk chunk = {};

	if (!this->compression[0])
		return;

	DEF_CHUNK(chunk, "zzfm");
	WRITE_TO_CHUNK(chunk);
	f_write(f, this->compression, 4);
	END_CHUNK();
}

int z64sample_impl_write_wav(z64sample* this, const char* name) {
	File __f = {}, * f = &__f;

	try {
		this->endian = SAMPLE_ENDIAN_LIL;
		f_write(f, "RIFF\0\0\0\0", 8);
		f_write(f, "WAVE", 4);

		write_fmt(this, f);
		write_fact(this, f);
		write_data(this, f);
		write_inst(this, f);
		write_smpl(this, f);
		write_zzbk(this, f);
		write_zzlp(this, f);
		write_zzfm(this, f);

		f_seek(f, 4);
		f_write(f, wrval(uint32_t, f->size - sizeof(WaveChunk)));
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
