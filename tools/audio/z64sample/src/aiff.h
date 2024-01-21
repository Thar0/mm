#ifndef AIFF_H
#define AIFF_H

#include "cclib/cclib-macros.h"
#include <cclib.h>
#include <stdint.h>

typedef struct DTA_BE {
	char     chunk_name[4];
	uint32_t chunk_size;
} AiffChunk;

typedef struct DTA_BE DTA_PACKED {
	AiffChunk; // 'COMM'
	uint16_t chan_num;
	uint16_t sample_num_h;
	uint16_t sample_num_l;
	uint16_t bit_depth;
	char sample_ratef80[10];
} AiffComm;

typedef struct DTA_BE DTA_PACKED {
	uint16_t index;
	uint16_t position_h;
	uint16_t position_l;
	uint8_t name_len;
	char name[];
} AiffMarkEntry;

typedef struct DTA_BE DTA_PACKED {
	AiffChunk; // 'MARK'
	uint16_t num;
	AiffMarkEntry entry[];
} AiffMark;

typedef struct DTA_BE {
	AiffChunk; // 'INST'
	int8_t  base_note;
	int8_t  fine_tune;
	int8_t  key_low;
	int8_t  key_hi;
	int8_t  vel_low;
	int8_t  vel_hi;
	int16_t gain;

	// 0 = sustain_loop
	// 1 = release_loop

	struct DTA_BE {
		uint16_t type;
		uint16_t start;
		uint16_t end;
	} loop[2];
} AiffInst;

typedef struct DTA_BE {
	AiffChunk; // 'SSND'
	uint32_t offset;
	uint32_t block_size;
	char     buf[];
} AiffSsnd;

#endif
