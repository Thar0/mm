#include <stdio.h>
#include <cclib.h>
#define STB_SPRINTF_DECORATE(name) cc_ ## name
#define STB_SPRINTF_IMPLEMENTATION
#include "cclib-io-stb.h"

static char s_buffer[1024 * 8];
const char* g_space_pad_512 = "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                ";
int g_tool_silent;

#define DO_RETURN return r

#define PRINT_IMPL(buffer, new_line) \
		int r = 0; \
		if (vanum > 1) { \
			va_list va; \
			va_start(va, fmt); \
			r = cc_vsnprintf(s_buffer, sizeof(s_buffer), fmt, va); \
			va_end(va); \
			fputs(s_buffer, stdout); \
			if (new_line) fputs("\n", stdout); \
		} else { \
			r = fputs(fmt ? fmt : "<NULL>", stdout); \
			if (new_line) fputs("\n", stdout); \
		} \
		DO_RETURN;

int __print(int vanum, const char* fmt, ...) {
	if (g_tool_silent) return 0;
	PRINT_IMPL(stdout, true);
}

int __print_align(int vanum, const char* a, const char* fmt, ...) {
	if (g_tool_silent) return 0;
	fprintf(stdout, "%-18s ", a);
	PRINT_IMPL(stdout, true);
}

int __printnnl(int vanum, const char* fmt, ...) {
	if (g_tool_silent) return 0;
	PRINT_IMPL(stdout, false);
}

int __printerr(int vanum, const char* fmt, ...) {
	PRINT_IMPL(stderr, true);
}

int __printerrnnl(int vanum, const char* fmt, ...) {
	PRINT_IMPL(stderr, false);
}

#undef DO_RETURN
#define DO_RETURN (void)0

int __print_exit(int vanum, const char* fmt, ...) {
	fputs("\a" COL_RED, stderr);
	PRINT_IMPL(stderr, true);
	fputs(COL_RESET, stderr);
	exit(1);
	(void)r;
}
