#ifndef TBL_ENUM
#define TBL_INTERNAL 1
#define TBL_ENUM(enum, value, ...) enum = value

typedef enum {
#endif

TBL_ENUM (SAMPLE_BIT_8,   8),
TBL_ENUM (SAMPLE_BIT_16,  16),
TBL_ENUM (SAMPLE_BIT_24,  24),
TBL_ENUM (SAMPLE_BIT_32,  32),
TBL_ENUM (SAMPLE_BIT_64,  64),

#if TBL_INTERNAL
SAMPLE_BIT_MAX
} SampleBitDepth;
#undef TBL_INTERNAL
#endif
#undef TBL_ENUM
