#define EXCEPTION_C
#include "cclib.h"
#include <stdarg.h>
#include <stdio.h>

#define TBL_ENUM(errno, message, ...) message
static const char* sExceptionError[] = {
#include "cclib-exception-tbl.h"
};

static int s_index = -1;
static struct __exception_t s_stack[64];
static bool s_mute;
ErrNo g_errno;
char* g_errstr;
char* g_errfile;
int g_errline;

struct __exception_t* __exception_push() {
	return memzero(&s_stack[++s_index], sizeof(struct __exception_t));
}

ErrNo __exception_pop() {
	g_errno = s_stack[s_index].no;
	g_errstr = s_stack[s_index].str;
	g_errline = s_stack[s_index].line;
	g_errfile = s_stack[s_index].file;
	s_index--;

	return g_errno;
}

void __exception_throw(ErrNo no, int line, char* file, int vanum, const char* fmt, ...) {
	if (s_index > -1) {
		if (no != ERR_NOERR) {
			s_stack[s_index].no = no;
			s_stack[s_index].file = file;
			s_stack[s_index].line = line;

			if (vanum > 0) {
				va_list va;
				va_start(va, fmt);
				s_stack[s_index].str = x_vfmt(fmt, va);
				va_end(va);
			} else
				s_stack[s_index].str = (char*)fmt;

		}
		longjmp(s_stack[s_index].jump, 1);
	} else {
		g_errstr = "failed to throw!";
		kill_by_exception();
	}
}

void mute_exception() {
	s_mute = true;
}

void unmute_exception() {
	s_mute = false;
}

void print_exception() {
	if (s_mute)
		return;

	if (g_errno == ERR_SILENT)
		return;

	fputs("\a", stderr);
	printerr(
		COL_YELLOW "\a%s" COL_RESET ": (%s:%d)",
		sExceptionError[g_errno], g_errfile, g_errline);
	if (g_errstr)
		printerr(
			"  %s",
			x_rep(g_errstr, "\n", "\n    "));
}
