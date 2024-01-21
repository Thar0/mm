#ifndef CCLIB_STRING_H
#define CCLIB_STRING_H

#include "cclib-types.h"
#include <string.h>

char* strchr(const char* src, int chr);
int strcmp(const char* src1, const char* src2);
char* strndup(const char* src, size_t n);
char* strdup(const char* src);
int strncasecmp(const char* src1, const char* src2, size_t n);
int strcasecmp(const char* src1, const char* src2);
char* strrchr(const char* src, int chr);
char* strpbrk(const char* str, const char* accept);
size_t strspn(const char* src, const char* accept);
size_t strcspn(const char* src, const char* reject);
char* strstr(const char* hay, const char* needle);
char* strnstr(const char* hay, size_t n, const char* needle);
char* strcasestr(const char* haystack, const char* needle);
char* strlwr(char* str);
char* strupr(char* str);
char* strflipsep(char* s);

size_t strcnlen(const char* str, size_t n);
size_t strclen(const char* str);
int strrep(char* src, const char* mtch, const char* rep);
int strcrep(const char* str, const char* mtch, const char* rep);
int strnrep(char* src, int len, const char* mtch, const char* rep);
int strislower(int chr);
int strisupper(int chr);
int strneq(const char* a, const char* b, size_t n);
int streq(const char* a, const char* b);

#if !__ide__
char* __m_strend(const char* src, const char* end, int elen);
char* __m_strnend(const char* src, const char* end, int elen, int n);
char* __m_strfend(const char* src, const char* end, int elen, int slen);
char* __m_strstart(const char* src, const char* begin, int blen);

#define strend(src, end)     __m_strend(src, end, strlen(end))
#define strnend(src, end, n) __m_strnend(src, end, strlen(end), n)
#define strfend(src, end, n) __m_strfend(src, end, strlen(end), n)
#define strstart(src, beg)   __m_strstart(src, beg, strlen(beg))
#else
char* strend(const char* src, const char* end);
char* strnend(const char* src, const char* end, int n);
char* strfend(const char* src, const char* end, int n);
char* strstart(const char* src, const char* begin);
#endif

bool chrspn(char c, const char* accept);

uint32_t strto_u32(const char* s);
char* fmtw_u32(char* p, uint32_t val);

#endif
