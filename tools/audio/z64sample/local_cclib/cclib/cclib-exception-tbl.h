#ifndef TBL_ENUM
#define TBL_INTERNAL 1
#define TBL_ENUM(errno, message) errno

typedef enum {
#endif

TBL_ENUM (ERR_NOERR,          "no-error"),
TBL_ENUM (ERR_RETHROW,        "rethrow"),
TBL_ENUM (ERR_SILENT,         ""),
TBL_ENUM (ERR_ERROR,          "error"),
TBL_ENUM (ERR_NULL,           "null"),
TBL_ENUM (ERR_NONIMPLEMENTED, "not implemented"),
TBL_ENUM (ERR_ALLOC,          "alloc"),
TBL_ENUM (ERR_REALLOC,        "realloc"),
TBL_ENUM (ERR_STRDUP,         "strdup"),
TBL_ENUM (ERR_MEMDUP,         "memdup"),
TBL_ENUM (ERR_FOPEN,          "fopen"),
TBL_ENUM (ERR_FCLOSE,         "fclose"),
TBL_ENUM (ERR_FREAD,          "fread"),
TBL_ENUM (ERR_FWRITE,         "fwrite"),
TBL_ENUM (ERR_MKDIR,          "mkdir"),
TBL_ENUM (ERR_STAT,           "stat"),
TBL_ENUM (ERR_OOB,            "out-of-bounds"),

TBL_ENUM (ERR_MAX,        "")

#if TBL_INTERNAL
} ErrNo;
#undef TBL_INTERNAL
#endif
#undef TBL_ENUM
