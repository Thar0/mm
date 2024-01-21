#include <cclib.h>
#include <ccmath.h>
#include "z64sample.h"
#include "vadpcm.h"

///////////////////////////////////////////////////////////////////////////////

static const f64 s_max_int8 = ((f64)__INT8_MAX__ + 1);
static const f64 s_max_int16 = ((f64)__INT16_MAX__ + 1);
static const f64 s_max_int24 = ((f64)__INT24_MAX__ + 1);
static const f64 s_max_int32 = ((f64)__INT32_MAX__ + 1);
static const f64 s_max_int64 = ((f64)__INT64_MAX__ + 1);

static void sample_read_int8(void* src, f64* dst) {
	*dst = *(int8_t*)src / s_max_int8;
}

static void sample_read_int16(void* src, f64* dst) {
	*dst = *(int16_t*)src / s_max_int16;
}

static void sample_read_int24(void* src, f64* dst) {
	*dst = ((int24_t*)src)->value / s_max_int24;
}

static void sample_read_int32(void* src, f64* dst) {
	*dst = *(int32_t*)src / s_max_int32;
}

static void sample_read_int64(void* src, f64* dst) {
	*dst = *(int64_t*)src / s_max_int64;
}

static void sample_read_f32(void* src, f64* dst) {
	*dst = *(float*)src;
}

static void sample_read_f64(void* src, f64* dst) {
	*dst = *(f64*)src;
}

///////////////////////////////////////////////////////////////////////////////

static void sample_write_int8(void* dst, f64* src) {
	*(int8_t*)dst = math_round_int(*src * s_max_int8);
}

static void sample_write_int16(void* dst, f64* src) {
	*(int16_t*)dst = math_round_int(*src * s_max_int16);
}

static void sample_write_int24(void* dst, f64* src) {
	((int24_t*)dst)->value = math_round_int(*src * s_max_int24);
}

static void sample_write_int32(void* dst, f64* src) {
	*(int32_t*)dst = math_round_int(*src * s_max_int32);
}

static void sample_write_int64(void* dst, f64* src) {
	*(int64_t*)dst = math_round_int(*src * s_max_int64);
}

static void sample_write_f32(void* dst, f64* src) {
	*(f32*)dst = *src;
}

static void sample_write_f64(void* dst, f64* src) {
	*(f64*)dst = *src;
}

///////////////////////////////////////////////////////////////////////////////

#define READ_BE(b, v, sz) \
		membswap(memcpy(b, v, sz), sz); \
		b += sz
#define READ_LE(b, v, sz) \
		memcpy(b, v, sz); \
		b += sz

static void data_read_chan1BE(z64sample* this, char* sample, int sample_size) {
	char* dst = this->samples_l;
	char* dst_end = dst + (sample_size * this->sample_num);

	for (; dst < dst_end;) {
		membswap(memcpy(dst, sample, sample_size), sample_size);

		dst += sample_size;
		sample += sample_size;
	}
}

static void data_read_chan2BE(z64sample* this, char* sample, int sample_size) {
	char* dst[2] = { this->samples_l, this->samples_r };
	char* dst_end = dst[0] + (sample_size * this->sample_num);

	for (; dst[0] < dst_end;) {
		membswap(memcpy(dst[0], sample, sample_size), sample_size);
		dst[0] += sample_size;
		sample += sample_size;
		membswap(memcpy(dst[1], sample, sample_size), sample_size);
		dst[1] += sample_size;
		sample += sample_size;
	}
}

static void data_read_chan1LE(z64sample* this, char* sample, int sample_size) {
	char* dst = this->samples_l;
	char* dst_end = dst + (sample_size * this->sample_num);

	for (; dst < dst_end;) {
		memcpy(dst, sample, sample_size);

		dst += sample_size;
		sample += sample_size;
	}
}

static void data_read_chan2LE(z64sample* this, char* sample, int sample_size) {
	char* dst[2] = { this->samples_l, this->samples_r };
	char* dst_end = dst[0] + (sample_size * this->sample_num);

	for (; dst[0] < dst_end;) {
		memcpy(dst[0], sample, sample_size);
		dst[0] += sample_size;
		sample += sample_size;
		memcpy(dst[1], sample, sample_size);
		dst[1] += sample_size;
		sample += sample_size;
	}
}

static void data_read_chan1vadpcm(z64sample* this, char* frame, int sample_size) {
	static int vadframe_size_tbl[SAMPLE_TYPE_MAX] = {
		[SAMPLE_TYPE_VAD9] = 9,
		[SAMPLE_TYPE_VAD5] = 5,
	};
	int vadframe_size = vadframe_size_tbl[this->type];
	char* frame_end = frame + (int)math_floorF((this->sample_num / 16.0) * vadframe_size);
	char* dst = this->samples_l;
	z64VadCodecContext ctx = {};

	if (!vadframe_size)
		throw(ERR_ERROR, "vadpcm frame size: vadframe_size_tbl[%d] = 0", this->type);

	for (; frame < frame_end; frame += vadframe_size, dst += sample_size * 16) {
		vadpcm_read_frame(&ctx, this->codebook_ctx, frame, vadframe_size, this->matching);
		memcpy(dst, ctx.guess, sample_size * 16);
	}

	this->type = SAMPLE_TYPE_INT;
}

///////////////////////////////////////////////////////////////////////////////

static void data_write_chan1BE(z64sample* this, File* f, int sample_size) {
	char* sample = this->samples_l;
	char* sample_end = sample + (this->sample_num * sample_size);

	for (; sample < sample_end; sample += sample_size)
		if (f_writeBE(f, sample, sample_size))
			throw(ERR_FWRITE, "failed to write sample of %d bytes", sample_size);
}

static void data_write_chan2BE(z64sample* this, File* f, int sample_size) {
	char* sample_l = this->samples_l;
	char* sample_r = this->samples_r;
	char* sample_l_end = sample_l + (this->sample_num * sample_size);

	for (; sample_l < sample_l_end; sample_l += sample_size, sample_r += sample_size) {
		if (f_writeBE(f, sample_l, sample_size))
			throw(ERR_FWRITE, "failed to write sample of %d bytes", sample_size);
		if (f_writeBE(f, sample_r, sample_size))
			throw(ERR_FWRITE, "failed to write sample of %d bytes", sample_size);
	}
}

static void data_write_chan1LE(z64sample* this, File* f, int sample_size) {
	char* sample = this->samples_l;
	char* sample_end = sample + (this->sample_num * sample_size);

	for (; sample < sample_end; sample += sample_size)
		if (f_write(f, sample, sample_size))
			throw(ERR_FWRITE, "failed to write sample of %d bytes", sample_size);
}

static void data_write_chan2LE(z64sample* this, File* f, int sample_size) {
	char* sample_l = this->samples_l;
	char* sample_r = this->samples_r;
	char* sample_l_end = sample_l + (this->sample_num * sample_size);

	for (; sample_l < sample_l_end; sample_l += sample_size, sample_r += sample_size) {
		if (f_write(f, sample_l, sample_size))
			throw(ERR_FWRITE, "failed to write sample of %d bytes", sample_size);
		if (f_write(f, sample_r, sample_size))
			throw(ERR_FWRITE, "failed to write sample of %d bytes", sample_size);
	}
}

static void data_write_chan1vadpcm(z64sample* this, File* f, int sample_size) {
	static int vadframe_size_tbl[SAMPLE_TYPE_MAX] = {
		[SAMPLE_TYPE_VAD9] = 9,
		[SAMPLE_TYPE_VAD5] = 5,
	};
	int vadframe_size = vadframe_size_tbl[this->type];
	char* sample = this->samples_l;
	char* sample_end = sample + this->sample_num * sample_size;
	z64VadCodecContext ctx = {};

	if (!this->codebook_ctx->coef_tbl)
		throw(ERR_ERROR, "coef_tbl");

	if (!vadframe_size)
		throw(ERR_ERROR, "vadpcm frame size: vadframe_size_tbl[%d] = 0", this->type);

	char* sample_loop_start = (this->loop_num && this->loop_end) ? (sample + (this->loop_start * sample_size)) : NULL;

	static uint8_t zero9[9] = {};

	int sample_num;
	for (; sample < sample_end; sample += sample_size * 16) {
		sample_num = clamp_max((sample_end - sample) / sample_size, 16);

		vadpcm_write_frame(&ctx, this->codebook_ctx, sample, vadframe_size, sample_num);

		if (sample_num < 16 && memeq(zero9, ctx.output, vadframe_size))
			break;

		f_write(f, ctx.output, vadframe_size);

		if (this->codebook_ctx->loop_seek == SIZE_MAX)
			continue;
		if (!sample_loop_start)
			continue;

		if ((sample + sample_size * 16) > sample_loop_start) {
			for (int i = 0; i < 16; i++)
				this->codebook_ctx->loop_state[i] = swapBE((int16_t)ctx.state[i]);

			f_seek(f, this->codebook_ctx->loop_seek);
			f_write(f, this->codebook_ctx->loop_state, sizeof(int16_t[16]));
			f_seek(f, FILE_END);
			sample_loop_start = NULL;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

z64DataReadFunc g_read_data_func_tbl[SAMPLE_TYPE_MAX][SAMPLE_ENDIAN_MAX][SAMPLE_CHANNEL_MAX] = {
	[SAMPLE_TYPE_INT][SAMPLE_ENDIAN_LIL][SAMPLE_CHANNEL_MONO] = data_read_chan1LE,
	[SAMPLE_TYPE_INT][SAMPLE_ENDIAN_LIL][SAMPLE_CHANNEL_STEREO] = data_read_chan2LE,
	[SAMPLE_TYPE_INT][SAMPLE_ENDIAN_BIG][SAMPLE_CHANNEL_MONO] = data_read_chan1BE,
	[SAMPLE_TYPE_INT][SAMPLE_ENDIAN_BIG][SAMPLE_CHANNEL_STEREO] = data_read_chan2BE,

	[SAMPLE_TYPE_FLOAT][SAMPLE_ENDIAN_LIL][SAMPLE_CHANNEL_MONO] = data_read_chan1LE,
	[SAMPLE_TYPE_FLOAT][SAMPLE_ENDIAN_LIL][SAMPLE_CHANNEL_STEREO] = data_read_chan2LE,
	[SAMPLE_TYPE_FLOAT][SAMPLE_ENDIAN_BIG][SAMPLE_CHANNEL_MONO] = data_read_chan1BE,
	[SAMPLE_TYPE_FLOAT][SAMPLE_ENDIAN_BIG][SAMPLE_CHANNEL_STEREO] = data_read_chan2BE,

	[SAMPLE_TYPE_VAD9][SAMPLE_ENDIAN_BIG][SAMPLE_CHANNEL_MONO] = data_read_chan1vadpcm,
	[SAMPLE_TYPE_VAD5][SAMPLE_ENDIAN_BIG][SAMPLE_CHANNEL_MONO] = data_read_chan1vadpcm,
};

z64DataWriteFunc g_write_data_func_tbl[SAMPLE_TYPE_MAX][SAMPLE_ENDIAN_MAX][SAMPLE_CHANNEL_MAX] = {
	[SAMPLE_TYPE_INT][SAMPLE_ENDIAN_LIL][SAMPLE_CHANNEL_MONO] = data_write_chan1LE,
	[SAMPLE_TYPE_INT][SAMPLE_ENDIAN_LIL][SAMPLE_CHANNEL_STEREO] = data_write_chan2LE,
	[SAMPLE_TYPE_INT][SAMPLE_ENDIAN_BIG][SAMPLE_CHANNEL_MONO] = data_write_chan1BE,
	[SAMPLE_TYPE_INT][SAMPLE_ENDIAN_BIG][SAMPLE_CHANNEL_STEREO] = data_write_chan2BE,

	[SAMPLE_TYPE_FLOAT][SAMPLE_ENDIAN_LIL][SAMPLE_CHANNEL_MONO] = data_write_chan1LE,
	[SAMPLE_TYPE_FLOAT][SAMPLE_ENDIAN_LIL][SAMPLE_CHANNEL_STEREO] = data_write_chan2LE,
	[SAMPLE_TYPE_FLOAT][SAMPLE_ENDIAN_BIG][SAMPLE_CHANNEL_MONO] = data_write_chan1BE,
	[SAMPLE_TYPE_FLOAT][SAMPLE_ENDIAN_BIG][SAMPLE_CHANNEL_STEREO] = data_write_chan2BE,

	[SAMPLE_TYPE_VAD9][SAMPLE_ENDIAN_BIG][SAMPLE_CHANNEL_MONO] = data_write_chan1vadpcm,
	[SAMPLE_TYPE_VAD5][SAMPLE_ENDIAN_BIG][SAMPLE_CHANNEL_MONO] = data_write_chan1vadpcm,
};

///////////////////////////////////////////////////////////////////////////////

z64SampleReadFunc g_read_sample_func_tbl[SAMPLE_TYPE_MAX][SAMPLE_BIT_MAX] = {
	[SAMPLE_TYPE_INT][8] = sample_read_int8,
	[SAMPLE_TYPE_INT][16] = sample_read_int16,
	[SAMPLE_TYPE_INT][24] = sample_read_int24,
	[SAMPLE_TYPE_INT][32] = sample_read_int32,
	[SAMPLE_TYPE_INT][64] = sample_read_int64,

	[SAMPLE_TYPE_FLOAT][32] = sample_read_f32,
	[SAMPLE_TYPE_FLOAT][64] = sample_read_f64,
};

z64SampleWriteFunc g_write_sample_func_tbl[SAMPLE_TYPE_MAX][SAMPLE_BIT_MAX] = {
	[SAMPLE_TYPE_INT][8] = sample_write_int8,
	[SAMPLE_TYPE_INT][16] = sample_write_int16,
	[SAMPLE_TYPE_INT][24] = sample_write_int24,
	[SAMPLE_TYPE_INT][32] = sample_write_int32,
	[SAMPLE_TYPE_INT][64] = sample_write_int64,

	[SAMPLE_TYPE_FLOAT][32] = sample_write_f32,
	[SAMPLE_TYPE_FLOAT][64] = sample_write_f64,
};
