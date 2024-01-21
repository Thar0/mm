#ifndef TBL_ENUM
#define TBL_INTERNAL 1
#define TBL_ENUM(enum, name, ...) enum

typedef enum {
#endif

TBL_ENUM (SAMPLE_CHANNEL_ZERO,   "zero"),
TBL_ENUM (SAMPLE_CHANNEL_MONO,   "mono"),
TBL_ENUM (SAMPLE_CHANNEL_STEREO, "stereo"),

#if TBL_INTERNAL
SAMPLE_CHANNEL_MAX
} SampleChannel;
#undef TBL_INTERNAL
#endif
#undef TBL_ENUM
