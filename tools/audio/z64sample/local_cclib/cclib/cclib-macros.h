#ifndef CCLIB_MACROS_H
#define CCLIB_MACROS_H

#include "cclib-types.h"

#define __va_num_(...)             __va_num_N(__VA_ARGS__)
#define __va_num_N( \
			_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, \
			_11, _12, _13, _14, _15, _16, _17, _18, _19, _20, \
			_21, _22, _23, _24, _25, _26, _27, _28, _29, _30, \
			_31, _32, _33, _34, _35, _36, _37, _38, _39, _40, \
			_41, _42, _43, _44, _45, _46, _47, _48, _49, _50, \
			_51, _52, _53, _54, _55, _56, _57, _58, _59, _60, \
			_61, _62, _63, N, ...) N
#define __va_seq_N() \
		63, 62, 61, 60, \
		59, 58, 57, 56, 55, 54, 53, 52, 51, 50, \
		49, 48, 47, 46, 45, 44, 43, 42, 41, 40, \
		39, 38, 37, 36, 35, 34, 33, 32, 31, 30, \
		29, 28, 27, 26, 25, 24, 23, 22, 21, 20, \
		19, 18, 17, 16, 15, 14, 13, 12, 11, 10, \
		9, 8, 7, 6, 5, 4, 3, 2, 1, 0
#define VA_NUM(...) __va_num_(__VA_ARGS__, __va_seq_N())

#define __for_va_join_impl(a, b)                                       a ## b
#define __for_va_join(a, b)                                            __for_va_join_impl(a, b)
#define __for_va1(macro, a)                                            macro(a)
#define __for_va2(macro, a, b)                                         macro(a) macro(b)
#define __for_va3(macro, a, b, c)                                      macro(a) macro(b) macro(c)
#define __for_va4(macro, a, b, c, d)                                   macro(a) macro(b) macro(c) macro(d)
#define __for_va5(macro, a, b, c, d, e)                                macro(a) macro(b) macro(c) macro(d) macro(e)
#define __for_va6(macro, a, b, c, d, e, f)                             macro(a) macro(b) macro(c) macro(d) macro(e) macro(f)
#define __for_va7(macro, a, b, c, d, e, f, g)                          macro(a) macro(b) macro(c) macro(d) macro(e) macro(f) macro(g)
#define __for_va8(macro, a, b, c, d, e, f, g, h)                       macro(a) macro(b) macro(c) macro(d) macro(e) macro(f) macro(g) macro(h)
#define __for_va9(macro, a, b, c, d, e, f, g, h, i)                    macro(a) macro(b) macro(c) macro(d) macro(e) macro(f) macro(g) macro(h) macro(i)
#define __for_va10(macro, a, b, c, d, e, f, g, h, i, j)                macro(a) macro(b) macro(c) macro(d) macro(e) macro(f) macro(g) macro(h) macro(i) macro(j)
#define __for_va11(macro, a, b, c, d, e, f, g, h, i, j, k)             macro(a) macro(b) macro(c) macro(d) macro(e) macro(f) macro(g) macro(h) macro(i) macro(j) macro(k)
#define __for_va12(macro, a, b, c, d, e, f, g, h, i, j, k, l)          macro(a) macro(b) macro(c) macro(d) macro(e) macro(f) macro(g) macro(h) macro(i) macro(j) macro(k) macro(l)
#define __for_va13(macro, a, b, c, d, e, f, g, h, i, j, k, l, m)       macro(a) macro(b) macro(c) macro(d) macro(e) macro(f) macro(g) macro(h) macro(i) macro(j) macro(k) macro(l) macro(m)
#define __for_va14(macro, a, b, c, d, e, f, g, h, i, j, k, l, m, n)    macro(a) macro(b) macro(c) macro(d) macro(e) macro(f) macro(g) macro(h) macro(i) macro(j) macro(k) macro(l) macro(m) macro(n)
#define __for_va15(macro, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) macro(a) macro(b) macro(c) macro(d) macro(e) macro(f) macro(g) macro(h) macro(i) macro(j) macro(k) macro(l) macro(m) macro(n) macro(o)
#define __for_va_(macro, M, ...)                                       M(macro, __VA_ARGS__)
#define __for_va_apply(macro, ...)                                     __for_va_(macro, __for_va_join(__for_va, VA_NUM(__VA_ARGS__)), __VA_ARGS__)
#define VA_FOREACH(macro, ...)                                         __for_va_apply(macro, __VA_ARGS__)

#define new(type) calloc(1, sizeof(type))

#define renew(p, old_type, new_type) ({ \
		void* n = crealloc(p, sizeof(old_type), sizeof(new_type)); \
		if (n) { p = n; } \
		n; \
	})

#if __ide__
void* delete(const void*, ...);
#else
#define __impl_for_each_free(a) if (a) { free((void*)a); a = NULL; }
#define delete(...)             ({ \
		VA_FOREACH(__impl_for_each_free, __VA_ARGS__) \
		NULL; \
	})
#endif

#define __impl_hash_a(a) (uint32_t)(
#define __impl_hash_b(a) *33 + a)
#define cchash(...)      VA_FOREACH(__impl_hash_a, __VA_ARGS__) 5381 VA_FOREACH(__impl_hash_b, __VA_ARGS__)

#define ptrval(type, value) ({ \
		type* v = x_alloc(sizeof(type)); \
		*v = (type) { value }; \
		v; \
	})

#define wrval(type, value) ptrval(type, value), sizeof(type)

// read size and post increment work pointer
#define readbuf(type, b) ({ \
		type* v = (void*)b; \
		b = (void*)(((char*)b) + sizeof(type)); \
		v; \
	})

#define cast_bit(bit_count, value) ({ \
		struct __cc_intbitstrct_ ## bit_count s = { value }; \
		s.v; \
	})

#if !__ide__
#define swapBE(val) ({ \
		__auto_type __var__ = (val); \
		switch (sizeof(__var__)) { \
				case 1: break; \
				case 2: __var__ = __builtin_bswap16(__var__); break; \
				case 4: __var__ = __builtin_bswap32(__var__); break; \
				case 8: __var__ = __builtin_bswap64(__var__); break; \
				default: membswap(&__var__, sizeof(__var__)); break; \
		} \
		__var__; \
	})
#else
int64_t swapBE(int64_t);
#endif

#ifdef __clang__
#define nested(type, name, args) \
		type (^ name) args = NULL; \
		name = ^ type args
#define nested_var(v)  typeof(v) v = (typeof(v)) 0;
#define nested_stru(v) typeof(v) v = {}

#else
#define nested(type, name, args) \
		type name args
#define nested_var(v)  (void)0
#define nested_stru(v) (void)0
#endif

#define arrlen(arr)              (sizeof(arr) / sizeof(arr[0]))
#define abs(a)                   ((a) < 0 ? -(a) : (a))
#define max(a, b)                ((a) > (b) ? (a) : (b))
#define min(a, b)                ((a) < (b) ? (a) : (b))
#define decr(x)                  (x -= (x > 0) ? 1 : 0)
#define alignval(val, align)     ((((val) % (align)) != 0) ? (val) + (align) - ((val) % (align)) : (val))
#define bitwrapshift(val, shift) ({ \
		typeof(val) v = val; \
		if ((shift) < 0) v = (v << abs(shift)) | (v >> (sizeof(v) * 8 - abs(shift))); \
		else v = (v >> (shift)) | (v << (sizeof(v) * 8 - (shift))); \
		v; \
	})

#define node_add(head, node) do { \
	typeof(node) * __n__ = &head; \
	while (*__n__) __n__ = &(*__n__)->next; \
	*__n__ = node; \
} while (0)

#define node_insert(head, node) do { \
	typeof(node) n = head->next; \
	head->next = node; \
	node->next = n; \
} while (0)

#define node_remove(head, node) do { \
	typeof(node) * __n__ = &head; \
	while (*__n__ != node) __n__ = &(*__n__)->next; \
	*__n__ = node->next; \
} while (0)

#define node_delete(head, node) do { \
	typeof(node) killNode = node; \
	node_remove(head, node); \
	delete(killNode); \
} while (0)

static inline
int64_t clamp(int64_t val, int64_t min, int64_t max) {
	return val < min ? min : val > max ? max : val;
}

static inline
int64_t clamp_max(int64_t val, int64_t max) {
	return val > max ? max : val;
}

static inline
int64_t clamp_min(int64_t val, int64_t min) {
	return val < min ? min : val;
}

static inline
uint64_t u_clamp(uint64_t val, uint64_t min, uint64_t max) {
	return val < min ? min : val > max ? max : val;
}

static inline
uint64_t u_clamp_max(uint64_t val, uint64_t max) {
	return val > max ? max : val;
}

static inline
uint64_t u_clamp_min(uint64_t val, uint64_t min) {
	return val < min ? min : val;
}

static inline
unsigned int rmask(unsigned int value, unsigned int mask) {
	unsigned int shift = __builtin_ctz(mask);

	value >>= shift;
	mask >>= shift;

	return value & mask;
}

static inline
unsigned int wmask(unsigned int value, unsigned int mask) {
	unsigned int shift = __builtin_ctz(mask);

	value <<= shift;

	return value & mask;
}

static inline
bool hasnullbyte16(uint16_t v) {
	uint16_t hi = 0x8080;
	uint16_t lo = 0x0101;
	uint16_t test = (v - lo) & (~v);

	return test & hi;
}
static inline
bool hasnullbyte32(uint32_t v) {
	uint32_t hi = 0x80808080;
	uint32_t lo = 0x01010101;
	uint32_t test = (v - lo) & (~v);

	return test & hi;
}
static inline
bool hasnullbyte64(uint64_t v) {
	uint64_t hi = 0x8080808080808080;
	uint64_t lo = 0x0101010101010101;
	uint64_t test = (v - lo) & (~v);

	return test & hi;
}

#define offsetptr(struct, member) \
		(void*)((char*)(&struct) + __builtin_offsetof(typeof(struct), member))

#define DTA_PACKED   __attribute__ ((__packed__))
#define DTA_BE       __attribute__((scalar_storage_order("big-endian")))
#define WARN_DISCARD __attribute__ ((__warn_unused_result__))

#if __ide__
#define onlaunch_fun_t __attribute__ ((constructor)) void
#define onexit_fun_t   __attribute__ ((destructor)) void
#else
#define onlaunch_fun_t __attribute__ ((constructor)) static void
#define onexit_fun_t   __attribute__ ((destructor)) static void
#endif

#define fornode(varname, head) \
		for (typeof(head) varname = head; varname; varname = varname->next)

#define __impl_paste(a, b) a ## b
#define __impl_ccassert(predicate, line) \
		typedef char (__impl_paste (____ccassert_line_, line))[2 * !!(predicate) - 1]
#define ccassert(predicate) __impl_ccassert(predicate, __LINE__)

#endif
