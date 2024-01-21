#ifndef CCLIB_H
#define CCLIB_H

#include "cclib/cclib-types.h"
#include "cclib/cclib-exception.h"
#include "cclib/cclib-macros.h"
#include "cclib/cclib-string.h"
#include "cclib/cclib-thread.h"
#include "cclib/cclib-io.h"

#if __ide__
#undef _WIN32
#endif

int lib_heapinit(size_t heap_size);
int lib_heapdump(void);
void* lib_alloc(int64_t);
void* lib_free(const void*);

void* x_alloc(size_t size);
void* x_memdup(const void* d, size_t n);
char* x_strndup(const char* s, size_t n);
char* x_strdup(const char* s);
char* x_enumify(const char* s);
char* x_snakeify(const char* s);
char* x_pascalify(const char* s);
char* x_camelify(const char* s);
char* x_strtrim(const char* s, const char* rej);
char* x_vfmt(const char* fmt, va_list va);
char* x_fmt(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
char* x_rep(const char* s, const char* f, const char* r);
char* x_filepath(const char* s);
char* x_filename(const char* s);
char* x_basename(const char* s);
char* x_filetypeas(const char* s, const char* e);

char* strdup(const char* s);
char* strndup(const char* s, size_t n);
void* memdup(const void* d, size_t n);
char* enumify(const char* s);
char* snakeify(const char* s);
char* pascalify(const char* s);
char* camelify(const char* s);
char* strtrim(const char* s, const char* rej);
char* vfmt(const char* fmt, va_list va);
char* fmt(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
char* rep(const char* s, const char* f, const char* r);
char* filepath(const char* s);
char* filename(const char* s);
char* basename(const char* s);
char* filetypeas(const char* s, const char* e);

#if !__ide__
char* __x_strjoin(const char* a, uint32_t b, uint32_t c, const char** d);
char* __strjoin(const char* a, uint32_t b, uint32_t c, const char** d);
#define x_strjoin(sep, ...) __x_strjoin(sep, strlen(sep), VA_NUM(__VA_ARGS__), (const char*[]) { __VA_ARGS__, NULL })
#define strjoin(sep, ...)   __strjoin(sep, strlen(sep), VA_NUM(__VA_ARGS__), (const char*[]) { __VA_ARGS__, NULL })
#else
char* x_strjoin(const char* sep, ...);
char* strjoin(const char* sep, ...);
#endif

f32 randf();
const char* rands();
const char* strbool(bool v);

int wrap(int x, int min, int max);
f32 fwrap(f32 x, f32 min, f32 max);

uint32_t hashmem(const void* p, size_t n);
uint32_t hashstr(const char* p);
void* crealloc(void* var, size_t prev, size_t new);
int memeq(const void* mema, const void* memb, size_t size);
void* memmem(const void* hay, size_t haylen, const void* nee, size_t neelen);
void* memzero(void* var, size_t s);
void* memset (void* __s, int __c, size_t __n);
void* memshift(void* psrc, int shift, size_t elem_size, size_t elem_num);
void* membswap(void* v, size_t size);
size_t memeqlen(const void*, const void*, size_t len);

struct optarg_f_load {bool bin;};
int f_open(File* this, const char* fname, struct optarg_f_load arg);
#define f_open(this, fname, ...) f_open(this, fname, (struct optarg_f_load) { __VA_ARGS__ })

struct optarg_f_save {bool no_overwrite : 1, mkdir : 1;};
int f_save(File* this, const char* fname, struct optarg_f_save arg);
#define f_save(this, fname, ...) f_save(this, fname, (struct optarg_f_save) { __VA_ARGS__ })

void f_destroy(File*);

void* f_seek(File*, size_t seek);
void* f_wind(File*, int32_t offset);
int f_write(File*, const void* data, size_t size);
int f_writeBE(File*, const void* data, size_t size);
int f_write_pad(File*, size_t size);
int f_align(File*, int align);
int f_print(File*, const char* fmt, ...);

struct optarg_f_newsym {const char* type_name; int align;};
File* f_newsym(File* this, const char* name, struct optarg_f_newsym);
#define f_newsym(this, name, ...) f_newsym(this, name, (struct optarg_f_newsym) { __VA_ARGS__ })

int f_write_ref(File* this, const void* name, struct SymFlag flags);
#define f_write_ref(this, name, ...) f_write_ref(this, name, (struct SymFlag) { __VA_ARGS__ })

int f_set_label(File* this, const char* name);

int f_link(File* this);
size_t f_get_sym_offset(File* this, const char* name);
const char* f_get_linker_syms(File* this);
const char* f_get_header_syms(File* this, int get_count(void*, const char*, size_t), void* udata);
int memdump(const void* mem, size_t s, const char* file);

#define f_with(var, file, ...) \
		for (File __scope_copy__ = (File) { .size = 1 }; __scope_copy__.size; f_destroy(&__scope_copy__)) \
		for (File* var = &__scope_copy__; var && !f_open(var, file, ## __VA_ARGS__); var = NULL)

///////////////////////////////////////////////////////////////////////////////

#if !__ide__
int __argtab_parse(void* udata, int ntab, const ArgTable* table, int narg, char** args);
void __argtab_help(int ntab, const ArgTable* table) __attribute__((noreturn));
#define argtab_parse(udata, table, narg, args) __argtab_parse(udata, arrlen(table), table, narg, args)
#define argtab_help(table)                     __argtab_help(arrlen(table), table)
#else
int argtab_parse(void* udata, const ArgTable* table, int narg, char** args);
void argtab_help(const ArgTable* table) __attribute__((noreturn));
#endif

///////////////////////////////////////////////////////////////////////////////

struct optarg_sys_stat {bool get_access : 1, get_mod : 1, get_status : 1, get_max : 1, get_min : 1;};
time_t sys_stat(const char* item, struct optarg_sys_stat);
#define sys_stat(item, ...) sys_stat(item, (struct optarg_sys_stat) { __VA_ARGS__ })
bool sys_isdir(const char* s);
bool sys_mkdir(const char* s);
ftime_t sys_ftime();

#endif
