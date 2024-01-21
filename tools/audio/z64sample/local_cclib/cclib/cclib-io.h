#ifndef CCLIB_IO_H
#define CCLIB_IO_H

#include "cclib-types.h"

int cc_vsprintf(char* buf, char const* fmt, va_list va);
int cc_vsnprintf(char* buf, int count, char const* fmt, va_list va);
int cc_sprintf(char* buf, char const* fmt, ...);
int cc_snprintf(char* buf, int count, char const* fmt, ...);

#if !__ide__
int __print(int, const char* fmt, ...);
int __print_align(int, const char* a, const char* fmt, ...);
int __printnnl(int, const char* fmt, ...);
int __printerr(int, const char* fmt, ...);
int __printerrnnl(int, const char* fmt, ...);
int __print_exit(int, const char* fmt, ...) __attribute__((noreturn));
#define print(...)          __print(VA_NUM(__VA_ARGS__), ## __VA_ARGS__)
#define print_align(a, ...) __print_align(VA_NUM(__VA_ARGS__), a, ## __VA_ARGS__)
#define printnnl(...)       __printnnl(VA_NUM(__VA_ARGS__), ## __VA_ARGS__)
#define printerr(...)       __printerr(VA_NUM(__VA_ARGS__), ## __VA_ARGS__)
#define printerrnnl(...)    __printerrnnl(VA_NUM(__VA_ARGS__), ## __VA_ARGS__)
#define print_exit(...)     __print_exit(VA_NUM(__VA_ARGS__), ## __VA_ARGS__)
#else
int print(const char* fmt, ...);
int print_align(const char* a, const char* fmt, ...);
int printnnl(const char* fmt, ...);
int printerr(const char* fmt, ...);
int printerrnnl(const char* fmt, ...);
int print_exit(const char* fmt, ...) __attribute__((noreturn));
#endif

#define COL_GRAY   "\e[90m"
#define COL_RED    "\e[91m"
#define COL_GREEN  "\e[92m"
#define COL_YELLOW "\e[93m"
#define COL_BLUE   "\e[94m"
#define COL_PURPLE "\e[95m"
#define COL_CYAN   "\e[96m"
#define COL_DARK   "\e[2m"
#define COL_RESET  "\e[m"

#if __MINGW64__
#define __fmt_selector__(_32, posix64, win64) win64
#elif __x86_64__
#define __fmt_selector__(_32, posix64, win64) posix64
#else
#define __fmt_selector__(_32, posix64, win64) _32
#endif

#define paste(f) f

#define FV_HEX_SIZE __fmt_selector__("0x%08uX", "0x%08luX", "0x%08lluX")

extern const char* g_space_pad_512;

#endif
