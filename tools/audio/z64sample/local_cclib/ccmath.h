#ifndef CCMATH_H
#define CCMATH_H

#include <cclib/cclib-types.h>

f64 math_lerp(f64 v, f64 min, f64 max);
f64 math_normalize(f64 v, f64 min, f64 max);
f32 math_remap(f32 v, f32 a_min, f32 a_max, f32 b_min, f32 b_max);
f64 math_powF(f64 v, f64 exp);
f64 math_logF(f64 v);
f64 math_log2F(f64 v);
f64 math_log10F(f64 v);
f64 math_ceilF(f64 v);
f64 math_floorF(f64 v);
f64 math_roundF(f64 v);
f64 math_round_int(f64 v);
f64 math_absF(f64 v);
f64 math_minF(f64 a, f64 b);
f64 math_maxF(f64 a, f64 b);

f64 math_wrapF(f64 v, f64 min, f64 max);
int64_t math_wrapS(int64_t v, int64_t min, int64_t max);
f64 math_pingpongF(f64 v, f64 min, f64 max);
int64_t math_pingpongS(int64_t v, int64_t min, int64_t max);

f64 math_align_min(f64 v, f64 inv_div);

#endif
