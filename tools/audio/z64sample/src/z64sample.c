#include <cclib.h>
#include <ccmath.h>
#include "z64sample.h"
#include "vadpcm.h"

///////////////////////////////////////////////////////////////////////////////

static int8_t s_type_bit_depth_support_tbl[SAMPLE_TYPE_MAX][SAMPLE_BIT_MAX] = {
	[SAMPLE_TYPE_INT] =         {
		[SAMPLE_BIT_8] = true,
		[SAMPLE_BIT_16] = true,
		[SAMPLE_BIT_24] = true,
		[SAMPLE_BIT_32] = true,
		[SAMPLE_BIT_64] = true,
	},
	[SAMPLE_TYPE_FLOAT] =       {
		[SAMPLE_BIT_32] = true,
		[SAMPLE_BIT_64] = true,
	},
	[SAMPLE_TYPE_VAD9] =      {
		[SAMPLE_BIT_16] = true,
	},
	[SAMPLE_TYPE_VAD5] = {
		[SAMPLE_BIT_16] = true,
	},
};

#define TBL_ENUM(enum, ...) [enum] = true
static int8_t s_bit_depth_support_tbl[0xFF] = {
#include "z64sample-bit-depth-tbl.h"
};

///////////////////////////////////////////////////////////////////////////////

extern int z64sample_impl_read_wav(z64sample*, const char* name);
extern int z64sample_impl_write_wav(z64sample*, const char* name);
extern int z64sample_impl_read_aiff(z64sample*, const char* name);
extern int z64sample_impl_write_aiff(z64sample*, const char* name);
extern int z64sample_impl_read_vadpcm(z64sample*, const char* name);
extern int z64sample_impl_write_vadpcm(z64sample*, const char* name);

#define TBL_ENUM(enum, ext, fext) [enum] = { "." # ext, z64sample_impl_read_ ## fext, z64sample_impl_write_ ## fext }
static struct {
	const char* ext;
	z64FileFunc read;
	z64FileFunc write;
} s_format_info_tbl[] = {
#include "z64sample-sample-format-tbl.h"
};

static void print_file_ext_err(const char* file) {
	char* dot = strrchr(file, '.');

	if (dot)
		printerr("unsupported file format: " COL_BLUE "%.*s" COL_RED "%s" COL_RESET, dot - file, file, dot);
	else
		printerr("missing extension: " COL_RESET "%s" COL_RESET, file);
}

static void print_operation(const char* file, const char* operation) {
	char* dot = strrchr(file, '.');

	print_align(operation, "%.*s" COL_GRAY "%s" COL_RESET, dot - file, file, dot);
}

int z64sample_read_file(z64sample* this, const char* file) {
	for (int i = 0; i < SAMPLE_FORMAT_MAX; i++) {
		if (strend(file, s_format_info_tbl[i].ext)) {
			print_operation(file, "read:");

			return s_format_info_tbl[i].read(this, file);
		}
	}

	print_file_ext_err(file);

	return EXIT_FAILURE;
}

int z64sample_write_file(z64sample* this, const char* file) {
	if (!this->samples_l) {
		printerr("no sample loaded, can't write!");
		return EXIT_FAILURE;
	}

	for (int i = 0; i < SAMPLE_FORMAT_MAX; i++) {
		if (strend(file, s_format_info_tbl[i].ext)) {
			print_operation(file, "write:");

			return s_format_info_tbl[i].write(this, file);
		}
	}

	print_file_ext_err(file);

	return EXIT_FAILURE;
}

///////////////////////////////////////////////////////////////////////////////

void z64sample_destroy(z64sample* this) {
	z64sample_destroy_codebook(this);
	delete(this->samples_l, this->samples_r, this->mark, this->tabledesign);
	*this = (z64sample) {};
}

///////////////////////////////////////////////////////////////////////////////

int z64sample_set_type_and_bit(z64sample* this, SampleType dst_sample_type, SampleBitDepth dst_bit_depth) {
	if (this->type == dst_sample_type && this->bit_depth == dst_bit_depth)
		return EXIT_SUCCESS;

	try {
		int dst_sample_size = dst_bit_depth / 8;
		char* dst_head[2] = {};
		char* dst[2] = {};

		int src_sample_size = this->bit_depth / 8;
		char* src[2] = { this->samples_l, this->samples_r };
		char* src_end = src[0] + (src_sample_size * this->sample_num);

		if (dst_sample_type == SAMPLE_TYPE_VAD9 || dst_sample_type == SAMPLE_TYPE_VAD5) {
			this->output_type = dst_sample_type;
			dst_sample_type = SAMPLE_TYPE_INT;
			dst_bit_depth = SAMPLE_BIT_16;
		}

		z64SampleReadFunc sample_read = g_read_sample_func_tbl[this->type][this->bit_depth];
		z64SampleWriteFunc sample_write = g_write_sample_func_tbl[dst_sample_type][dst_bit_depth];

		if (this->type != dst_sample_type)
			print_align("convert type:", COL_BLUE "%-6s " COL_GRAY "->" COL_BLUE " %s" COL_RESET, g_type_name_tbl[this->type], g_type_name_tbl[dst_sample_type]);
		if (this->bit_depth != dst_bit_depth)
			print_align("convert bit:", COL_BLUE "%-6s " COL_GRAY "->" COL_BLUE " %s" COL_RESET, g_bit_name_tbl[this->bit_depth], g_bit_name_tbl[dst_bit_depth]);

		for (int i = 0; i < this->chan_num; i++)
			dst[i] = dst_head[i] = new(char[dst_sample_size * this->sample_num]);

		switch (this->chan_num) {
			case SAMPLE_CHANNEL_MONO: {
				for (; src[0] < src_end;) {
					f64 sample;

					sample_read(src[0], &sample);
					sample_write(dst[0], &sample);

					dst[0] += dst_sample_size;
					src[0] += src_sample_size;
				}
				break;
			}

			case SAMPLE_CHANNEL_STEREO: {
				for (; src[0] < src_end;) {
					f64 sample;

					sample_read(src[0], &sample);
					sample_write(dst[0], &sample);

					sample_read(src[1], &sample);
					sample_write(dst[1], &sample);

					dst[0] += dst_sample_size;
					dst[1] += dst_sample_size;

					src[0] += src_sample_size;
					src[1] += src_sample_size;
				}
				break;
			}

			default:
				throw(ERR_ERROR, "fatal error!");
		}

		delete(this->samples_l, this->samples_r);
		this->samples_l = dst_head[0];
		this->samples_r = dst_head[1];

		this->bit_depth = dst_bit_depth;
		this->type = dst_sample_type;
	} except {
		print_exception();
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
};

int z64sample_normalize(z64sample* this) {
	try {
		int sample_size = this->bit_depth / 8;
		char* src[2] = { this->samples_l, this->samples_r };
		char* src_end = src[0] + (sample_size * this->sample_num);

		z64SampleReadFunc sample_read = g_read_sample_func_tbl[this->type][this->bit_depth];
		z64SampleWriteFunc sample_write = g_write_sample_func_tbl[this->type][this->bit_depth];

		f64 max = 0;

		switch (this->chan_num) {
			case SAMPLE_CHANNEL_MONO: {
				for (; src[0] < src_end; src[0] += sample_size) {
					f64 sample;

					sample_read(src[0], &sample);
					max = max(max, math_absF(sample));
				}
				break;
			}

			case SAMPLE_CHANNEL_STEREO: {
				for (; src[0] < src_end; src[0] += sample_size, src[1] += sample_size) {
					f64 sample;

					sample_read(src[0], &sample);
					max = max(max, math_absF(sample));

					sample_read(src[1], &sample);
					max = max(max, math_absF(sample));
				}
				break;
			}

			default:
				throw(ERR_ERROR, "fatal error!");
				break;
		}

		if (max == 0.0 || max == 1.0)
			break_exception;

		f64 mul = 1.0 / max;

		switch (this->chan_num) {
			case SAMPLE_CHANNEL_MONO: {
				for (; src[0] < src_end; src[0] += sample_size) {
					f64 sample;

					sample_read(src[0], &sample);
					sample *= mul;
					sample_write(src[0], &sample);
				}
				break;
			}

			case SAMPLE_CHANNEL_STEREO: {
				for (; src[0] < src_end; src[0] += sample_size, src[1] += sample_size) {
					f64 sample;

					sample_read(src[0], &sample);
					sample *= mul;
					sample_write(src[0], &sample);

					sample_read(src[1], &sample);
					sample *= mul;
					sample_write(src[1], &sample);
				}
				break;
			}

			default:
				throw(ERR_ERROR, "fatal error!");
				break;
		}
	} except {
		print_exception();
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

#include "ebur128.h"

static int (*s_ebu128_add_frames_func_tbl[SAMPLE_TYPE_MAX][SAMPLE_BIT_MAX])(ebur128_state* st, const void* src, size_t frames) = {
	[SAMPLE_TYPE_INT][SAMPLE_BIT_16] = (void*)ebur128_add_frames_short,
	[SAMPLE_TYPE_INT][SAMPLE_BIT_32] = (void*)ebur128_add_frames_int,
	[SAMPLE_TYPE_FLOAT][SAMPLE_BIT_32] = (void*)ebur128_add_frames_float,
	[SAMPLE_TYPE_FLOAT][SAMPLE_BIT_64] = (void*)ebur128_add_frames_double,
};

static f64 calc_mono_loudness(z64sample* this) {
	if (!s_ebu128_add_frames_func_tbl[this->type][this->bit_depth])
		throw(ERR_ERROR, "fatal error: unsupported type for loudness measurement!\n  %s %s",
			g_type_name_tbl[this->type],
			g_bit_name_tbl[this->bit_depth]);

	ebur128_state* ebu = ebur128_init(1, this->sample_rate, EBUR128_MODE_I);

	if (s_ebu128_add_frames_func_tbl[this->type][this->bit_depth](ebu, this->samples_l, this->sample_num)) {
		ebur128_destroy(&ebu);
		throw(ERR_ERROR, "fatal error: failed to process frames for loudness measurement!");
	}

	f64 loudness = 0;

	ebur128_loudness_global(ebu, &loudness);
	ebur128_destroy(&ebu);

	return loudness;
}

static f64 calc_stereo_loudness(z64sample* this) {
	if (!s_ebu128_add_frames_func_tbl[this->type][this->bit_depth])
		throw(ERR_ERROR, "fatal error: unsupported type for loudness measurement!\n  %s %s",
			g_type_name_tbl[this->type],
			g_bit_name_tbl[this->bit_depth]);

	ebur128_state* ebu = ebur128_init(2, this->sample_rate, EBUR128_MODE_I);

	int sample_size = (this->bit_depth / 8);
	char* data = new(char[sample_size * this->sample_num * 2]);
	char* p = data;

	for (uint i = 0; i < this->sample_num; i++) {
		memcpy(p, &((char*)this->samples_l)[i * sample_size], sample_size); p += sample_size;
		memcpy(p, &((char*)this->samples_r)[i * sample_size], sample_size); p += sample_size;
	}

	if (s_ebu128_add_frames_func_tbl[this->type][this->bit_depth](ebu, data, this->sample_num)) {
		ebur128_destroy(&ebu);
		delete(data);
		throw(ERR_ERROR, "fatal error: failed to process frames for loudness measurement!");
	}

	f64 loudness = 0;

	ebur128_loudness_global(ebu, &loudness);
	ebur128_destroy(&ebu);
	delete(data);

	return loudness;
}

static void box_filter(const f32* src, f32* dst, int len, int win) {
	int half = win / 2;

	for (int i = 0; i < len; i++) {
		f64 sum = 0;

		for (int j = i - half; j <= i + half; j++)
			if (j >= 0 && j < len)
				sum += src[j];

		dst[i] = sum / win;
	}
}

static void limiter(z64sample* this, f64 gain) {
	z64SampleReadFunc sample_read = g_read_sample_func_tbl[this->type][this->bit_depth];
	z64SampleWriteFunc sample_write = g_write_sample_func_tbl[this->type][this->bit_depth];
	int sample_size = this->bit_depth / 8;
	char* sample = this->samples_l;
	char* sample_end = sample + this->sample_num * sample_size;

	f32* exceed_tbl;
	f32* filter_tbl;
	const int window = 16;
	const int half_window = window / 2;
	const f64 threshold = 0.98;
	const f64 msec_per_sample = (1.0 / this->sample_rate) * 1000;

	if (!(exceed_tbl = new(f32[this->sample_num + window])))
		throw(ERR_ERROR, "new(f32[this->sample_num])");
	if (!(filter_tbl = new(f32[this->sample_num + window])))
		throw(ERR_ERROR, "new(f32[this->sample_num])");

	f32* tbl = exceed_tbl;

	f64 hold_msec = 0;
	f64 max = 0;

	for (; sample < sample_end; sample += sample_size, tbl++, hold_msec += msec_per_sample) {
		f64 l = 0;

		sample_read(sample, &l);
		l *= gain;
		sample_write(sample, &l);

		f32 over = math_maxF(math_absF(l) - threshold, 0.0);

		if (over > max) {
			hold_msec = 0;
			max = over;
		} else if (hold_msec > 30.0) {
			hold_msec = 0;
			max = over;
		}

		*tbl = max;
	}

	box_filter(exceed_tbl, filter_tbl, this->sample_num + window, window);

	tbl = filter_tbl + half_window;
	sample = this->samples_l;
	for (; sample < sample_end; sample += sample_size, tbl++) {
		f64 l;

		if (tbl < filter_tbl)
			continue;

		sample_read(sample, &l);
		l *= threshold / (*tbl + threshold);
		sample_write(sample, &l);
	}

	delete(exceed_tbl, filter_tbl);
}

static void stereo_to_mono(z64sample* this) {
	SampleType original_type = this->type;
	SampleBitDepth original_bitdepth = this->bit_depth;

	if (z64sample_set_type_and_bit(this, SAMPLE_TYPE_FLOAT, SAMPLE_BIT_64))
		throw(ERR_ERROR, "fatal error: could not temporarily convert type and bit depth!");

	z64SampleReadFunc sample_read = g_read_sample_func_tbl[this->type][this->bit_depth];
	z64SampleWriteFunc sample_write = g_write_sample_func_tbl[this->type][this->bit_depth];
	f64 stereo_loudness = calc_stereo_loudness(this);

	int sample_size = this->bit_depth / 8;
	char* sample_l = this->samples_l;
	char* sample_r = this->samples_r;
	char* sample_end = sample_l + this->sample_num * sample_size;
	char* mono_head;
	char* mono_dst = mono_head = new(char[this->sample_num * sample_size]);

	for (; sample_l < sample_end; sample_l += sample_size, sample_r += sample_size, mono_dst += sample_size) {
		f64 l, r, d;

		sample_read(sample_l, &l);
		sample_read(sample_r, &r);
		d = math_lerp(0.5, l, r);
		sample_write(mono_dst, &d);
	}

	delete(this->samples_l, this->samples_r);
	this->samples_l = mono_head;
	this->chan_num = SAMPLE_CHANNEL_MONO;

	f64 mono_loudness = calc_mono_loudness(this);
	f64 gain = math_powF(10.0, (stereo_loudness - mono_loudness) / 20.0);

	print_align("gain:", "%.2f dB", gain);

	limiter(this, gain);

	if (z64sample_set_type_and_bit(this, original_type, original_bitdepth))
		throw(ERR_ERROR, "fatal error: could not recover type and bit depth!");
}

static void mono_to_stereo(z64sample* this) {
	int sample_size = this->bit_depth / 8;

	this->samples_r = memdup(this->samples_l, this->sample_num * sample_size);
	this->chan_num = SAMPLE_CHANNEL_STEREO;
}

int z64sample_set_channel_num(z64sample* this, SampleChannel chan_num) {
	try {
		if (this->chan_num == chan_num)
			break_exception;

		switch (this->chan_num) {
			case SAMPLE_CHANNEL_STEREO: stereo_to_mono(this); break;
			case SAMPLE_CHANNEL_MONO:   mono_to_stereo(this); break;
			default:
				throw(ERR_ERROR, "fatal error: can't set channel num to %d", chan_num);
				break;
		}
	} except {
		print_exception();
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

void z64sample_print_state(z64sample* this) {

	print_align("state:", "%s %s %s %s",
		g_endian_name_tbl[this->endian],
		g_bit_name_tbl[this->bit_depth],
		g_type_name_tbl[this->type],
		g_chan_name_tbl[this->chan_num]);
}

///////////////////////////////////////////////////////////////////////////////

int z64_is_supported_bit_depth(uint8_t bit_depth) {
	return s_bit_depth_support_tbl[bit_depth];
}

int z64_is_supported_type_bit_depth(SampleType type, SampleBitDepth bit_depth) {
	return s_type_bit_depth_support_tbl[type][bit_depth];
}

///////////////////////////////////////////////////////////////////////////////

#define TBL_ENUM(enum, str, help) [enum] { str, help, strlen(str) }
struct {
	char* str;
	char* help;
	int   len;
} s_sample_type_info_tbl[] = {
#include "z64sample-type-tbl.h"
};

void z64_print_sample_type_tbl() {
	static char empty[128] = "                                                                                                                                ";
	int max_len = 0;

	for (uint32_t i = 0; i < SAMPLE_TYPE_MAX; i++)
		max_len = max(max_len, s_sample_type_info_tbl[i].len + 2);

	for (uint32_t i = 0; i < SAMPLE_TYPE_MAX; i++) {
		int pad = max_len - s_sample_type_info_tbl[i].len;
		print(COL_GRAY "   " COL_BLUE "%s" COL_GRAY "%.*s" "%s" COL_RESET, s_sample_type_info_tbl[i].str, pad, empty, s_sample_type_info_tbl[i].help);
	}
}

SampleBitDepth z64_get_sample_bit_depth(const char* s) {
	uint8_t bit_depth = strtoull(s, NULL, 10);

	if (s_bit_depth_support_tbl[bit_depth])
		return bit_depth;
	return SAMPLE_BIT_MAX;
}

SampleType z64_get_sample_type(const char* s) {
	int len = strlen(s);

	for (uint32_t i = 0; i < SAMPLE_TYPE_MAX; i++)
		if (s_sample_type_info_tbl[i].len == len)
			if (memeq(s_sample_type_info_tbl[i].str, s, len + 1))
				return i;

	return SAMPLE_TYPE_MAX;
}

#define TBL_ENUM(enum, name, _unk) [enum] = name
const char* g_type_name_tbl[SAMPLE_TYPE_MAX] = {
#include "z64sample-type-tbl.h"
};
#define TBL_ENUM(enum, name) [enum] = #name "-bit"
const char* g_bit_name_tbl[SAMPLE_BIT_MAX] = {
#include "z64sample-bit-depth-tbl.h"
};
#define TBL_ENUM(enum, name) [enum] = name
const char* g_chan_name_tbl[SAMPLE_CHANNEL_MAX] = {
#include "z64sample-channel-tbl.h"
};
#define TBL_ENUM(enum, name) [enum] = name
const char* g_endian_name_tbl[SAMPLE_ENDIAN_MAX] = {
#include "z64sample-endian-tbl.h"
};
