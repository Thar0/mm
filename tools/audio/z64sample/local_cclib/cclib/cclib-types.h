#ifndef CCLIB_INT_H
#define CCLIB_INT_H

#include <stdlib.h>
#include <stdbool.h>

///////////////////////////////////////////////////////////////////////////////

#if __MINGW32__ || __MINGW64__ || __clang__
typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef unsigned int       uint;
#else
typedef __int8_t   int8_t;
typedef __int16_t  int16_t;
typedef __int32_t  int32_t;
typedef __int64_t  int64_t;
typedef __uint8_t  uint8_t;
typedef __uint16_t uint16_t;
typedef __uint32_t uint32_t;
typedef __uint64_t uint64_t;
#endif

///////////////////////////////////////////////////////////////////////////////

typedef __builtin_va_list va_list;
typedef float             f32;
typedef double            f64;
typedef double            ftime_t;

#define __INT24_MAX__ 8388607
#define __INT24_MIN__ -8388608

typedef struct __attribute__((packed, gcc_struct)) int24_t {
	int32_t value : 24;
} int24_t;

typedef struct __attribute__((packed, gcc_struct)) uint24_t {
	uint32_t value : 24;
} uint24_t;

///////////////////////////////////////////////////////////////////////////////

struct SymFlag {
	enum { REF_32 =  0, REF_16  = 1 } size   : 1;
	enum { REF_ABS = 0, REF_REL = 1 } type   : 1;
	enum { REF_LO =  1, REF_HI  = 2 } split  : 3;
	enum { REF_BE =  0, REF_LE  = 1 } endian : 1;
};

typedef struct Symbol Symbol;
typedef struct File {
	uint8_t __pad[4];
	union {char* data; void* v_data;};
	size_t size;
	size_t cap;
	size_t seek;
	struct LinkFile* link;
	struct LinkInfo* info;
} File;

#define argtab_entry_cmd(STR, METHOD, HELP, ...) { \
			.type = 'a', \
			.s = STR, \
			.l = strlen(STR), \
			.method = METHOD, \
			.help = { HELP, strlen(HELP), "" __VA_ARGS__ }, \
}
#define argtab_entry_null()                      { \
			.type = 'n' \
}
#define argtab_entry_title(STR)                   { \
			.type = 't', \
			.help.s = STR, \
			.help.l = strlen(STR), \
}
#define argtab_entry_unk(METHOD)                 { \
			.type = 'u', \
			.method = METHOD, \
}
#define argtab_entry_any(METHOD)                 { \
			.type = 'y', \
			.method = METHOD, \
}

typedef int (*ArgTabFunc)(void* user_data, char* cur_arg, int narg, char** arg);

#if 0
static int argtab_func_example(void* udata, char* cmd, int n, char** arg) {

	return /* amount to skip for next arg */;
}
#endif

typedef struct ArgTable {
	char*      s;
	ArgTabFunc method;
	struct {
		char*    s;
		uint16_t l;
		char*    d;
	} help;
	uint8_t type;
	uint8_t l;
} ArgTable;

#define FILE_END SIZE_MAX

#endif
