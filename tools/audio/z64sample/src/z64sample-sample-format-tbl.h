#ifndef TBL_ENUM
#define TBL_INTERNAL 1
#define TBL_ENUM(enum, extension, function_extension) enum

typedef enum {
#endif

TBL_ENUM (SAMPLE_FORMAT_WAVE,      wav,       wav),
TBL_ENUM (SAMPLE_FORMAT_AIFF,      aiff,      aiff),
TBL_ENUM (SAMPLE_FORMAT_AIFC,      aifc,      vadpcm),

#if TBL_INTERNAL
SAMPLE_FORMAT_MAX
} SampleFormat;
#undef TBL_INTERNAL
#endif
#undef TBL_ENUM
