#ifndef TBL_ENUM
#define TBL_INTERNAL 1
#define TBL_ENUM(enum, ...) enum

typedef enum {
#endif

TBL_ENUM (SAMPLE_ENDIAN_BIG, "big-endian"),
TBL_ENUM (SAMPLE_ENDIAN_LIL, "little-endian"),

#if TBL_INTERNAL
SAMPLE_ENDIAN_MAX
} SampleEndian;
#undef TBL_INTERNAL
#endif
#undef TBL_ENUM
