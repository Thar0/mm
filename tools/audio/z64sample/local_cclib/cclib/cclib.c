#include "cclib.h"
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdint.h>
#include <limits.h>
#include <sys/time.h>

ccassert(1 == sizeof(int8_t));
ccassert(1 == sizeof(uint8_t));
ccassert(2 == sizeof(int16_t));
ccassert(2 == sizeof(uint16_t));
ccassert(4 == sizeof(int32_t));
ccassert(4 == sizeof(uint32_t));
ccassert(8 == sizeof(int64_t));
ccassert(8 == sizeof(uint64_t));
ccassert(8 == sizeof(int64_t));

#if !__ide__
ccassert(3 == sizeof(int24_t));
ccassert(3 == sizeof(uint24_t));
#endif

///////////////////////////////////////////////////////////////////////////////

extern const char* g_tool_name;
extern const char* g_tool_version;
extern int g_tool_silent;

#ifdef _WIN32
#include <windows.h>
#endif

onlaunch_fun_t app_launch(int narg, char** arg) {
#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
	system("");
#else

	/*
	 *  Mingw does not seem like it
	 *  supports construct function
	 *  having argument stuff
	 */

	if (narg > 1) {
		for (; *arg; arg++) {
			if (**arg == '-') {
				char* a = *arg + strspn(*arg, "-");

				if (memeq(a, "silent", strlen("silent") + 1)) {
					g_tool_silent = true;
					break;
				}
			}
		}
	}
#endif

#ifndef __ccnotitle__
	char buffer[1024];
	char* p = buffer;

	int third = 12;
	int pad = 2;
	int dec = 3;
	int len_tn = strlen(g_tool_name);
	int len_tv = strlen(g_tool_version);
	int len = dec + pad + len_tn + 1 + len_tv + third + dec;

	#define COPY(s)     memcpy(p, s, strlen(s)); p += strlen(s)
	#define SET(s, len) memset(p, s, len); p += len
	#define DEC "[*]"

	COPY("" COL_GRAY DEC);
	SET('-', len - dec * 2);
	COPY("" DEC "\n");

	COPY(" | " COL_BLUE);
	SET(' ', pad);
	COPY(g_tool_name);
	COPY(" " COL_GRAY);
	COPY(g_tool_version);
	COPY(COL_GRAY);
	SET(' ', third);
	COPY(" |\n");

	COPY(DEC);
	SET('-', len - dec * 2);
	COPY("" DEC COL_RESET "\n");
	SET('\0', 4);

	print(buffer);
#endif
}

///////////////////////////////////////////////////////////////////////////////

static struct {
	char*       head;
	size_t      offset;
	size_t      max;
	size_t      f;
	const char* header;
} s_bufx = {
	.max    = 0x400000,
	.f      =  100000,

	.header = "xbuf\xDE\xAD\xBE\xEF"
};

// # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

onlaunch_fun_t x_init() {
	struct timeval sTime;

	gettimeofday(&sTime, NULL);
	s_bufx.head = calloc(1, s_bufx.max);
	srand(sTime.tv_usec + sTime.tv_sec);
}

onexit_fun_t x_dest() {
	delete(s_bufx.head);
}

void* x_alloc(size_t size) {
	char* ret;

	if (size <= 0)
		return NULL;

	if (s_bufx.offset + size + 10 >= s_bufx.max)
		s_bufx.offset = 0;

	// Write Header
	ret = &s_bufx.head[s_bufx.offset];
	memcpy(ret, s_bufx.header, 8);
	s_bufx.offset += 8;

	// Write Data
	ret = &s_bufx.head[s_bufx.offset];
	s_bufx.offset += size + 1;

	return memset(ret, 0, size + 1);
}

static void* m_alloc(size_t s) {
	return calloc(1, s);
}

///////////////////////////////////////////////////////////////////////////////

typedef void* (*MAL)(size_t);

static inline char* ifyize(char* new, const char* str, int (*tocase)(int)) {
	size_t w = 0;
	int c = 1;
	const size_t len = strlen(str);

	enum {
		NONE = 0,
		IS_LOWER,
		IS_UPPER,
		NONALNUM,
	} prev = NONE, this = NONE;

	for (size_t i = 0; i < len; i++) {
		const char chr = str[i];

		if (isalnum(chr)) {
			if (isupper(chr))
				this = IS_UPPER;
			else
				this = IS_LOWER;

			if (tocase) {
				if (this == IS_UPPER && prev == IS_LOWER)
					new[w++] = '_';
				else if (prev == NONALNUM)
					new[w++] = '_';
				new[w++] = tocase(chr);
			} else {
				if (this == IS_UPPER && prev == IS_LOWER)
					c = 1;
				else if (prev == NONALNUM)
					c = 1;

				if (c)
					new[w++] = toupper(chr);
				else
					new[w++] = tolower(chr);
				c = 0;
			}

		} else
			this = NONALNUM;

		prev = this;
	}

	return new;
}

static char* __impl_memdup(MAL falloc, const char* data, size_t size) {
	if (!data || !size)
		return NULL;

	return memcpy(falloc(size), data, size);
}

static char* __impl_strndup(MAL falloc, const char* s, size_t n) {
	if (!n || !s || n < 0) return NULL;
	return memcpy (falloc(n + 1), s, strnlen(s, n));
}

static char* __impl_strdup(MAL falloc, const char* s) {
	if (!s) return NULL;
	int len = strlen(s);
	return memcpy(falloc(len + 1), s, len);
}

static char* __impl_enumify(MAL falloc, const char* s) {
	char* new = falloc(strlen(s) * 2);

	return ifyize(new, s, toupper);
}

static char* __impl_snakeify(MAL falloc, const char* s) {
	char* new = falloc(strlen(s) * 2);

	return ifyize(new, s, tolower);
}

static char* __impl_pascalify(MAL falloc, const char* s) {
	char* new = falloc(strlen(s) * 2);

	return ifyize(new, s, NULL);
}

static char* __impl_camelify(MAL falloc, const char* s) {
	char* new = falloc(strlen(s) * 2);

	ifyize(new, s, NULL);
	new[0] = tolower(new[0]);

	return new;
}

static char* __impl_strtrim(MAL falloc, const char* item, const char* reject) {
	int slen;
	int len = slen = strlen(item);
	char* n = falloc(len);
	int lead = strspn(item, reject);
	int trail = 0;

	nested(bool, chrspn, (int c, const char* accept)) {
		for (; *accept; accept++)
			if (c == *accept)
				return true;
		return false;
	};

	while (chrspn(item[--len], reject))
		trail++;

	return memcpy(n, item + lead, slen - lead - trail);
}

static char* __impl_fmt(MAL falloc, const char* fmt, va_list va) {
	char buf[1024 * 8];
	int len;

	len = cc_vsnprintf(buf, sizeof(buf), fmt, va);

	return __impl_memdup(falloc, buf, len + 1);
}

static char* __impl_rep(MAL falloc, const char* s, const char* find, const char* rep) {
	int d = strcrep(s, find, rep);
	size_t len = strlen(s);
	char* n = __impl_strndup(falloc, s, len + max(d, 0));

	memcpy(n, s, len + 1);
	strrep(n, find, rep);

	return n;
}

static char* __impl_filepath(MAL falloc, const char* s) {
	char* n = strflipsep(__impl_strdup(falloc, s));
	char* l = strrchr(n, '/');

	if (l) *l = '\0';

	return n;
}

static char* __impl_filename(MAL falloc, const char* s) {
	char* n = strflipsep(__impl_strdup(falloc, s));
	char* l = strrchr(n, '/');

	if (l) return l + 1;
	return n;
}

static char* __impl_basename(MAL falloc, const char* s) {
	char* n = __impl_filename(falloc, s);

	if (*n) {
		char* l = strrchr(n, '.');
		char* p = strrchr(n, '/');

		if (l && (!p || l > p)) *l = '\0';
	}

	return n;
}

static char* __impl_filetypeas(MAL falloc, const char* s, const char* ext) {
	char* n = __impl_strndup(falloc, s, strlen(s) + strlen(ext) + 1);
	char* l = strrchr(n, '.');
	char* p = strrchr(n, '/');

	try {
		if (*ext != '.') {
			throw(ERR_ERROR,
				"%sfiletypeas(\"" COL_GREEN "%s" COL_RESET "\", \"" COL_RED "." COL_GREEN "%s" COL_RESET "\");\n"
				"Second argument is missing a dot!",
				falloc == x_alloc ? "x_" : "",
				s, ext
			);
		}
	} except {
		kill_by_exception();
	}

	if (l && (!p || l > p)) {
		*l = '\0';
		memmove(l, ext, strlen(ext));
	} else
		strcat(n, ext);

	return n;
}

static char* __impl_strjoin(MAL falloc, const char* sep, uint32_t sep_len, uint32_t list_num, const char** __list) {
	int len = sep_len * (list_num - 1);
	uint32_t list_len[list_num];
	const char** list = __list;
	uint32_t* ll = list_len;

	for (; *list; list++, ll++) {
		*ll = strlen(*list);
		len += *ll;
	}

	char* str = falloc(len + 1);
	char* p = str;

	list = __list;
	ll = list_len;

	for (; *list; list++, ll++) {
		memcpy(p, *list, *ll);
		p += *ll;

		if (*(list + 1)) {
			memcpy(p, sep, sep_len);
			p += sep_len;
		}
	}
	*p = '\0';

	return str;
}

//crustify
void* x_memdup(const void* d, size_t n)          { return __impl_memdup(x_alloc, d, n); }
char* x_strndup(const char* s, size_t n)         { return __impl_strndup(x_alloc, s, n); }
char* x_strdup(const char* s)                    { return __impl_strdup(x_alloc, s); }
char* x_enumify(const char* s)                   { return __impl_enumify(x_alloc, s); }
char* x_snakeify(const char* s)                  { return __impl_snakeify(x_alloc, s); }
char* x_pascalify(const char* s)                 { return __impl_pascalify(x_alloc, s); }
char* x_camelify(const char* s)                  { return __impl_camelify(x_alloc, s); }
char* x_strtrim(const char* s, const char* rej)  { return __impl_strtrim(x_alloc, s, rej); }
char* x_vfmt(const char* fmt, va_list va)        { char* r = __impl_fmt(x_alloc, fmt, va); return r;}
char* x_fmt(const char* fmt, ...)                { va_list va; va_start(va, fmt); char* r = __impl_fmt(x_alloc, fmt, va); va_end(va);return r;}
char* x_rep(const char* s, const char* f, const char* r)  { return __impl_rep(x_alloc, s, f, r); }
char* x_filepath(const char* s)                  { return __impl_filepath(x_alloc, s); }
char* x_filename(const char* s)                  { return __impl_filename(x_alloc, s); }
char* x_basename(const char* s)                  { return __impl_basename(x_alloc, s); }
char* x_filetypeas(const char* s, const char* e) { return __impl_filetypeas(x_alloc, s, e); }
char* __x_strjoin(const char* a, uint32_t b, uint32_t c, const char** d) { return __impl_strjoin(x_alloc, a, b, c, d); }

char* strdup(const char* s)                      { return __impl_strdup(m_alloc, s); }
char* strndup(const char* s, size_t n)           { return __impl_strndup(m_alloc, s, n); }
void* memdup(const void* d, size_t n)            { return __impl_memdup(m_alloc, d, n); }
char* enumify(const char* s)                     { return __impl_enumify(m_alloc, s); }
char* snakeify(const char* s)                    { return __impl_snakeify(m_alloc, s); }
char* pascalify(const char* s)                   { return __impl_pascalify(m_alloc, s); }
char* camelify(const char* s)                    { return __impl_camelify(m_alloc, s); }
char* strtrim(const char* s, const char* rej)    { return __impl_strtrim(m_alloc, s, rej); }
char* vfmt(const char* fmt, va_list va)          { char* r = __impl_fmt(m_alloc, fmt, va); return r;}
char* fmt(const char* fmt, ...)                  { va_list va; va_start(va, fmt); char* r = __impl_fmt(m_alloc, fmt, va); va_end(va);return r;}
char* rep(const char* s, const char* f, const char* r)    { return __impl_rep(m_alloc, s, f, r); }
char* filepath(const char* s)                    { return __impl_filepath(m_alloc, s); }
char* filename(const char* s)                    { return __impl_filename(m_alloc, s); }
char* basename(const char* s)                    { return __impl_basename(m_alloc, s); }
char* filetypeas(const char* s, const char* e)   { return __impl_filetypeas(m_alloc, s, e); }
char* __strjoin(const char* a, uint32_t b, uint32_t c, const char** d)   { return __impl_strjoin(m_alloc, a, b, c, d); }
//uncrustify

///////////////////////////////////////////////////////////////////////////////

const char* strbool(bool v) {
	return v ? "" COL_GREEN "true" COL_RESET : COL_RED "false" COL_RESET;
}

f32 randf() {
	return (f32)rand() / (f32)__INT32_MAX__;
}

const char* rands() {
	static char* map = "ABCDEFGHIJKLMNOPQRSTUVXYZabcdefghijklmnopqrstuvxyz0123456789_";
	const int map_len = strlen(map);
	int rnd = rand();
	int len = max(abs(rnd) % 16, 4);
	char* r;
	char* s = r = x_alloc(len * 4 + 1);
	char* e = s + len;

	for (; s < e; s++)
		*s = map[rand() % map_len];

	return r;
}

///////////////////////////////////////////////////////////////////////////////

int wrap(int x, int min, int max) {
	int range = max - min;

	if (x < min)
		x += range * ((min - x) / range + 1);

	return min + (x - min) % range;
}

f32 fwrap(f32 x, f32 min, f32 max) {
	f32 range = max - min;

	if (x < min)
		x += range * round((min - x) / range + 1);

	return min + fmod((x - min), range);
}

///////////////////////////////////////////////////////////////////////////////

uint32_t hashmem(const void* p, size_t n) {
	uint32_t hash = 5381;
	const char* c = p;
	const uint32_t* u = (void*)c;

	for (; n >= 4; u++, n -= 4) {
		c = (void*)u;

		hash = ((hash << 5) + hash) + c[0];
		hash = ((hash << 5) + hash) + c[1];
		hash = ((hash << 5) + hash) + c[2];
		hash = ((hash << 5) + hash) + c[3];
	}

	c = (void*)u;

	for (; n; c++, n--)
		hash = ((hash << 5) + hash) + *c;

	return hash;
}

uint32_t hashstr(const char* p) {
	uint32_t hash = 5381;
	const uint8_t* c = (void*)p;

	for (; *c && ((intptr_t)c) & 3; c++)
		hash += ((hash << 5) + hash) + *c;

	const uint32_t* u = (void*)c;

	for (;; u++) {
		c = (void*)u;

		if (hasnullbyte32(*u)) {

			if (!c[0]) return hash;
			hash = ((hash << 5) + hash) + c[0];

			if (!c[1]) return hash;
			hash = ((hash << 5) + hash) + c[1];

			if (!c[2]) return hash;
			hash = ((hash << 5) + hash) + c[2];

			if (!c[3]) return hash;
			hash = ((hash << 5) + hash) + c[3];

			continue;
		}

		hash = ((hash << 5) + hash) + c[0];
		hash = ((hash << 5) + hash) + c[1];
		hash = ((hash << 5) + hash) + c[2];
		hash = ((hash << 5) + hash) + c[3];
	}

	return hash;
}

void* crealloc(void* var, size_t prev, size_t new) {
	var = realloc(var, new);

	if (new > prev)
		memzero(((char*)var) + prev, new - prev);

	return var;
}

int memeq(const void* mema, const void* memb, size_t n) {
	const int* ia = mema;
	const int* ib = memb;

	for (; n >= 4; ia++, ib++, n -= 4)
		if (*ia != *ib)
			return 0;

	const char* ca = (void*)ia;
	const char* cb = (void*)ib;

	for (; n; ca++, cb++, n--)
		if (*ca != *cb)
			return 0;

	return true;
}

void* memmem(const void* hay, size_t haylen, const void* nee, size_t neelen) {
	char* bf = (char*) hay, * pt = (char*) nee, * p = bf;

	if (haylen < neelen || !bf || !pt || !haylen || !neelen)
		return NULL;

	while ((haylen - (p - bf)) >= neelen) {
		if (NULL != (p = memchr(p, *pt, haylen - (p - bf)))) {
			if (memeq(p, nee, neelen))
				return p;
			++p;
		} else
			break;
	}

	return NULL;
}

void* memzero(void* p, size_t n) {
	char* c = p;

	for (; n && ((intptr_t)c) & 3; c++, n--)
		*c = 0;

	uint32_t* i = (void*)c;

	for (; n >= 4; i++, n -= 4)
		*i = 0;

	c = (void*)i;

	for (; n; c++, n--)
		*c = 0;

	return p;
}

void* memshift(void* psrc, int shift, size_t elem_size, size_t elem_num) {
	char* src = psrc;
	size_t size = elem_num * elem_size;

	shift = wrap(shift, -elem_num, elem_num);

	if (shift == 0)
		return psrc;

	int rshift = wrap(-shift * elem_size, 0, size);
	shift = wrap(shift * elem_size, 0, size);

	char* part = memcpy(malloc(size - rshift), src + rshift, size - rshift);

	memmove(src + shift, src, size - shift);
	memcpy(src, part, size - rshift);
	free(part);

	return psrc;
}

void* membswap(void* v, size_t size) {
	switch (size) {
		case 1: {
			return v;
		}

		case 2: {
			*(uint16_t*)v = __builtin_bswap16(*(uint16_t*)v);
			return v;
		}

		case 4: {
			*(uint32_t*)v = __builtin_bswap32(*(uint32_t*)v);
			return v;
		}

		case 8: {
			*(uint64_t*)v = __builtin_bswap64(*(uint64_t*)v);
			return v;
		}

		default: {
			uint8_t* a = v;
			uint8_t* b = a + size - 1;

			for (; a < b ; a++, b--) {
				uint8_t t = *a;
				*a = *b;
				*b = t;
			}

			return v;
		}
	}

}

size_t memeqlen(const void* _a, const void* _b, size_t len) {
	const uint32_t* a32 = _a;
	const uint32_t* b32 = _b;
	size_t n = 0;

	for (; *a32 == *b32 && len >= 4; n += 4, len -= 4, a32++, b32++);

	const char* a = (char*)a32;
	const char* b = (char*)b32;

	for (; *a == *b && len; n++, len--, a++, b++);

	return n;
}

///////////////////////////////////////////////////////////////////////////////

void __argtab_help(int ntab, const ArgTable* table) {
	const ArgTable* tabend = table + ntab;
	int pad_arg = 8;
	int pad_help = 12;

	for (const ArgTable* tab = table; tab < tabend; tab++) {
		if (tab->type == 't') {
			print(" %s", x_rep(tab->help.s, "\n", "\n "));
		}

		if (tab->type == 'n') {
			print("");
			continue;
		}

		if (tab->type != 'a') continue;

		if (tab->help.l)
			printnnl("   " COL_GRAY "--" COL_BLUE "%-*s  " COL_GRAY "%-*s  " COL_RESET, pad_arg, tab->s, pad_help, tab->help.s);
		else
			printnnl("   " COL_GRAY "--" COL_BLUE "%-*s  " COL_RESET, pad_arg + pad_help + 2, tab->s);

		if (tab->help.d && *tab->help.d) {
			bool has_padded = true;
			char* msg = tab->help.d;
			int line_len = strcspn(msg, "\n");

			for (;;) {
				if (!has_padded)
					printnnl("\n%.*s", pad_arg + pad_help + 9, g_space_pad_512);
				printnnl("%.*s", line_len, msg);
				has_padded = false;

				msg += line_len;
				if (!*(msg++)) break;
				line_len = strcspn(msg, "\n");
			}
		}

		printnnl("\n");
	}

	print(" standard:");
	print("   " COL_GRAY "--" COL_BLUE "%-*s  %.*s  " COL_RESET "%s", pad_arg, "help", pad_help, g_space_pad_512, "this page");
	print("   " COL_GRAY "--" COL_BLUE "%-*s  %.*s  " COL_RESET "%s", pad_arg, "silent", pad_help, g_space_pad_512, "silence stdout");
	exit(1);
}

int __argtab_parse(void* udata, int ntab, const ArgTable* table, int narg, char** list_args) {
	list_args++;
	narg--;

	const ArgTable* tab_end = table + ntab;
	const ArgTable* tab_any = NULL;
	const ArgTable* tab_unk = NULL;
	char** argend = list_args + narg;

	try {
		// find special methods
		for (const ArgTable* tab = table; tab < tab_end; tab++) {
			if (tab->type == 'y') {
				tab_any = tab;
				if (tab_unk) break;
			} else if (tab->type == 'u') {
				tab_unk = tab;
				if (tab_unk) break;
			}
		}

		// check for help argument
		for (char** list = list_args; list < argend; list++) {
			if (**list != '-') continue;
			char* cmd = *list + strspn(*list, "-");

			if (memeq(cmd, "help", 5)) {
				__argtab_help(ntab, table);

				return_e EXIT_FAILURE;
			}
		}

		for (char** list = list_args; list < argend; list++) {
			int n = 0;

			// count non-cmd's
			for (char** tlist = list + 1; tlist < argend; tlist++, n++)
				if (**tlist == '-')
					break;

			if (**list == '-') {
				char* cmd = *list + strspn(*list, "-");
				const ArgTable* tab;

				for (tab = table; tab < tab_end; tab++) {
					if (tab->type != 'a') continue;
					if (!memeq(cmd, tab->s, tab->l + 1)) continue;

					list += tab->method(udata, cmd, n, list + 1);
					break;
				}

				if (memeq(cmd, "silent", strlen("silent") + 1)) {
					g_tool_silent = true;

					continue;
				}

				// earlier for loop didn't make any hits
				if (tab >= tab_end) {
					if (tab_unk)
						list += tab_unk->method(udata, cmd, n, list + 1);
					else
						throw(ERR_ERROR, "unknown argument: " COL_GRAY "--" COL_RED "%s" COL_RESET, cmd);
				}

				continue;
			}

			if (tab_any)
				list += tab_any->method(udata, *list, n, list + 1);
		}
	} except {
		print_exception();

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////

#undef sys_stat
time_t sys_stat(const char* s, struct optarg_sys_stat list) {
	struct stat st = { 0 };
	time_t t = 0;
	int f = 0;

	if (s == NULL)
		return 0;

	if (strend(s, "/") || strend(s, "\\")) {
		f = 1;
		s = strdup(s);
		((char*)s)[strlen(s) - 1] = 0;
	}

	if (stat(s, &st) == -1)
		goto done;

	int r = list.get_max + list.get_min + list.get_access + list.get_status + list.get_mod;

	if (r)
		throw(ERR_ERROR,
			"sys_stat(\"" COL_GREEN "%s" COL_RESET "\"):\n" COL_YELLOW
			"    two opt vars set to true!\n" COL_RESET
			"    .get_max=%s\n"
			"    .get_min=%s\n"
			"    .get_access=%s\n"
			"    .get_status=%s\n"
			"    .get_mod=%s",
			s,
			strbool(list.get_max),
			strbool(list.get_min),
			strbool(list.get_access),
			strbool(list.get_status),
			strbool(list.get_mod)
		);

	if (list.get_max)
		t = max(st.st_ctime, max(st.st_mtime, st.st_atime));

	else if (list.get_min)
		t = min(st.st_ctime, min(st.st_mtime, st.st_atime));

	else if (list.get_access)
		t = st.st_atime;

	else if (list.get_status)
		t = st.st_ctime;

	else if (list.get_mod)
		t = st.st_mtime;

	else
		t = st.st_mtime;

	done:
	if (f) delete(s);

	return t;
}

bool sys_isdir(const char* s) {
	size_t len = strlen(s);
	char tmp[len + 1];
	struct stat st = { 0 };

	memcpy(tmp, s, len + 1);
	strrep(tmp, "\\", "/");

	if (tmp[len - 1] == '/')
		tmp[len - 1] = '\0';

	if (stat(tmp, &st) == -1)
		return false;
	if (S_ISDIR(st.st_mode))
		return true;

	return false;
}

bool sys_mkdir(const char* s) {
	char* p = NULL;
	size_t len = strlen(s);
	char tmp[len + 1];

	memcpy(tmp, s, len + 1);
	strrep(tmp, "\\", "/");

	if (tmp[len - 1] == '/')
		tmp[len - 1] = 0;
	for (p = tmp + 1; *p; p++)
		if (*p == '/') {
			*p = 0;
			mkdir(tmp
#ifndef _WIN32
				, S_IRWXU
#endif
			);
			*p = '/';
		}
	mkdir(tmp
#ifndef _WIN32
		, S_IRWXU
#endif
	);

	return !sys_isdir(tmp);
}

ftime_t sys_ftime() {
	struct timeval sTime;

	gettimeofday(&sTime, NULL);

	return (ftime_t)sTime.tv_sec + ((ftime_t)sTime.tv_usec) / 1000000.0f;
}

///////////////////////////////////////////////////////////////////////////////
