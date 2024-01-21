#ifndef Z64SAMPLE_H
#define Z64SAMPLE_H

#include <cclib.h>
#include <stdint.h>

#if !__ide__
typedef __float80 float80;
#else
typedef float float80;
#endif

#include "z64sample-type-tbl.h"

#include "z64sample-sample-format-tbl.h"
#include "z64sample-bit-depth-tbl.h"
#include "z64sample-endian-tbl.h"
#include "z64sample-channel-tbl.h"

// hack of getting framesize for these
#define VADPCM_FRAME_FULL 9
#define VADPCM_FRAME_HALF 5
#define VADPCM_BIT_FULL   (9 * 8)
#define VADPCM_BIT_HALF   (5 * 8)

typedef struct z64TableDesign {
	int    order;
	int    bits;
	int    refine_iters;
	f64    thresh;
	int8_t print;
} z64TableDesign;

typedef struct z64sample {
	void* samples_l;
	void* samples_r;

	char           compression[4];
	SampleType     output_type;
	SampleType     type;
	SampleEndian   endian;
	SampleChannel  chan_num;
	SampleBitDepth bit_depth;

	uint32_t calc_sample_num;
	uint32_t sample_num;
	uint32_t sample_rate;

	uint32_t loop_start;
	uint32_t loop_end;
	uint32_t loop_num;

	uint8_t base_note;
	int8_t  fine_tune;
	uint8_t key_hi;
	uint8_t key_low;
	uint8_t vel_hi;
	uint8_t vel_low;

	uint8_t   num_mark;
	uint32_t* mark;

	struct z64CodebookContext* codebook_ctx;
	struct z64TableDesign*     tabledesign;
	void*  external_book;
	int8_t matching;
} z64sample;

typedef void (*z64SampleReadFunc)(void* src, f64* dst);
typedef void (*z64SampleWriteFunc)(void* dst, f64* src);
typedef void (*z64ChunkReadFunc)(z64sample*, void*);
typedef void (*z64ChunkWriteFunc)(z64sample*, File*);
typedef void (*z64DataReadFunc)(z64sample*, char*, int);
typedef void (*z64DataWriteFunc)(z64sample*, File*, int);

typedef int (*z64FileFunc)(z64sample*, const char*);

extern z64DataReadFunc g_read_data_func_tbl[SAMPLE_TYPE_MAX][SAMPLE_ENDIAN_MAX][SAMPLE_CHANNEL_MAX];
extern z64DataWriteFunc g_write_data_func_tbl[SAMPLE_TYPE_MAX][SAMPLE_ENDIAN_MAX][SAMPLE_CHANNEL_MAX];
extern z64SampleReadFunc g_read_sample_func_tbl[SAMPLE_TYPE_MAX][SAMPLE_BIT_MAX];
extern z64SampleWriteFunc g_write_sample_func_tbl[SAMPLE_TYPE_MAX][SAMPLE_BIT_MAX];

int z64sample_read_file(z64sample*, const char* file) WARN_DISCARD;
int z64sample_write_file(z64sample*, const char* file) WARN_DISCARD;

void z64sample_destroy(z64sample*);

int z64sample_set_type_and_bit(z64sample*, SampleType type, SampleBitDepth bit_depth) WARN_DISCARD;
int z64sample_normalize(z64sample*) WARN_DISCARD;
int z64sample_set_channel_num(z64sample* this, SampleChannel chan_num) WARN_DISCARD;
void z64sample_print_state(z64sample* this);

int z64_is_supported_bit_depth(uint8_t bit_depth) WARN_DISCARD;
int z64_is_supported_type_bit_depth(SampleType type, SampleBitDepth bit_depth) WARN_DISCARD;

void z64_print_sample_type_tbl();
SampleBitDepth z64_get_sample_bit_depth(const char* s);
SampleType z64_get_sample_type(const char* s);

extern const char* g_type_name_tbl[SAMPLE_TYPE_MAX];
extern const char* g_bit_name_tbl[SAMPLE_BIT_MAX];
extern const char* g_chan_name_tbl[SAMPLE_CHANNEL_MAX];
extern const char* g_endian_name_tbl[SAMPLE_ENDIAN_MAX];

void z64sample_tabledesign(z64sample* this, z64TableDesign* design);
int z64sample_read_codebook(z64sample* this, void* p);
void z64sample_destroy_codebook(z64sample* this);

#endif
