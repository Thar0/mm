#ifndef TBL_ENUM
#define TBL_INTERNAL 1
#define TBL_ENUM(enum, ...) enum

typedef enum {
#endif

TBL_ENUM (SAMPLE_TYPE_INT,         "int",         "< *.aiff, *.wav >"),
TBL_ENUM (SAMPLE_TYPE_FLOAT,       "float",       "< *.wav >"),
TBL_ENUM (SAMPLE_TYPE_VAD9,      "vadpcm",      "< *.aifc >"),
TBL_ENUM (SAMPLE_TYPE_VAD5, "vadpcm-half", "< *.half.aifc >"),

#if TBL_INTERNAL
SAMPLE_TYPE_MAX
} SampleType;
#undef TBL_INTERNAL
#endif
#undef TBL_ENUM
