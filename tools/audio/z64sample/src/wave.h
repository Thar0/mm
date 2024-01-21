#ifndef WAVE_H
#define WAVE_H

#include <cclib/cclib-types.h>

typedef enum {
	WAVE_LOOP_FORWARD,
	WAVE_LOOP_PINGPONG,
	WAVE_LOOP_REVERSE,
} EnumWaveLoop;

typedef enum {
	WAVE_TYPE_PCM        = 1,
	WAVE_TYPE_FLOAT      = 3,
	WAVE_TYPE_ALAW       = 6,
	WAVE_TYPE_MULAW      = 7,
	WAVE_TYPE_EXTENSIBLE = 0xFFFE,
} EnumWaveType;

typedef struct {
	char     chunk_name[4];
	uint32_t chunk_size;
} WaveChunk;

typedef struct {
	WaveChunk; // 'fmt '
	uint16_t type;
	uint16_t chan_num;
	uint32_t sample_rate;
	uint32_t byte_rate;
	uint16_t block_align;
	uint16_t bit_depth;
} WaveFmt;

typedef struct {
	WaveChunk; // 'fact'
	uint32_t sample_num;
} WaveFact;

typedef struct {
	WaveChunk; // 'data'
	char buf[];
} WaveData;

typedef struct {
	WaveChunk; // 'smpl'
	uint32_t manufacturer;
	uint32_t product;
	uint32_t sample_period;
	uint32_t unity_note;
	char     _pad[3];
	uint8_t  fine_tune;
	uint32_t format;
	uint32_t offset;
	uint32_t num_sample_loops;
	uint32_t sampler_data;
} WaveSmpl;

typedef struct {
	uint32_t cue_point_index;
	uint32_t type;
	uint32_t start;
	uint32_t end;
	uint32_t fraction;
	uint32_t num;
} WaveSampleLoop;

typedef struct {
	WaveChunk; // 'inst'
	int8_t base_note;
	int8_t fine_tune;
	int8_t gain;
	int8_t key_low;
	int8_t key_hi;
	int8_t vel_low;
	int8_t vel_hi;
} WaveInst;

#endif
