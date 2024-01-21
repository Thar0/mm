#ifndef VADPCM_H
#define VADPCM_H

#include "z64sample.h"

typedef int32_t*** CoefTable;

typedef struct z64CodebookContext {
	CoefTable coef_tbl;
	int16_t   order;
	int16_t   npred;
	int16_t*  raw;
	int16_t   loop_state[16];
	size_t    loop_seek;
} z64CodebookContext;

typedef struct z64VadCodecContext {
	uint8_t output[9];
	int32_t state[16];
	int16_t guess[16];
} z64VadCodecContext;

void vadpcm_read_frame(z64VadCodecContext*, z64CodebookContext*, void* src, int frame_size, int8_t matching);
void vadpcm_write_frame(z64VadCodecContext* ctx, z64CodebookContext* book, void* src, int frame_size, int sample_num);

#endif
