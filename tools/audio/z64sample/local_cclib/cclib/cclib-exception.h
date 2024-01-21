#ifndef CCLIB_EXCEPTION_H
#define CCLIB_EXCEPTION_H

#include <setjmp.h>
#include "cclib-exception-tbl.h"

struct __exception_t {
	jmp_buf jump;
	ErrNo   no;
	char*   str;
	char*   file;
	int     line;
};

struct __exception_t* __exception_push();
ErrNo __exception_pop();
void mute_exception();
void unmute_exception();
void print_exception();

#ifndef EXCEPTION_C
extern const ErrNo g_errno;
extern const char* g_errstr;
extern const char* g_errfile;
extern const int g_errline;
#endif

//crustify
#define try \
    { \
		struct __exception_t* __cptn__ = __exception_push(); \
		if (!setjmp(__cptn__->jump))
#define except \
	} \
	if (__exception_pop() != ERR_NOERR)
//uncrustify

#if __ide__
void throw(ErrNo, ...) __attribute__((noreturn));
void rethrow();
void kill_by_exception();
#else
void __exception_throw(ErrNo, int, char*, int, const char*, ...)
__attribute__((noreturn, format(printf, 5, 6)));
#define throw(no, ...)      __exception_throw(no, __LINE__, __FILE__, VA_NUM(__VA_ARGS__) - 1, ## __VA_ARGS__, NULL)
#define rethrow()           __exception_throw(ERR_RETHROW, __LINE__, __FILE__, 0, NULL)
#define kill_by_exception() do { \
			print_exception(); \
			printerr( \
				"> " COL_RED "kill at" COL_RESET ": (%s:%d)", \
				__FILE__, __LINE__); \
			exit(g_errno ? g_errno : -1); \
} while (0)
#endif

#define return_e return __exception_pop(),
#define break_exception  throw(ERR_NOERR)

#endif
