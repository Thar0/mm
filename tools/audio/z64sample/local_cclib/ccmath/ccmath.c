#include <ccmath.h>
#include <math.h>

/////////////////////////////////////////////////////////////////////////////

f64 math_lerp(f64 v, f64 min, f64 max) {
	return min + v * (max - min);
}

f64 math_normalize(f64 v, f64 min, f64 max) {
	return (v - min) / (max - min);
}

f32 math_remap(f32 v, f32 a_min, f32 a_max, f32 b_min, f32 b_max) {
	return b_min + ((v - a_min) / (a_max - a_min)) * (b_max - b_min);
}

f64 math_powF(f64 v, f64 exp) {
	return pow(v, exp);
}

f64 math_logF(f64 v) {
	return log(v);
}

f64 math_log2F(f64 v) {
	return log2(v);
}

f64 math_log10F(f64 v) {
	return log10(v);
}

f64 math_ceilF(f64 v) {
	return ceil(v);
}

f64 math_floorF(f64 v) {
	return floor(v);
}

f64 math_roundF(f64 v) {
	return round(v);
}

f64 math_round_int(f64 v) {
	return rint(v);
}

f64 math_absF(f64 v) {
	return fabs(v);
}

f64 math_minF(f64 a, f64 b) {
	return fmin(a, b);
}

f64 math_maxF(f64 a, f64 b) {
	return fmax(a, b);
}

/////////////////////////////////////////////////////////////////////////////

f64 math_wrapF(f64 v, f64 min, f64 max) {
	f64 range = max - min;

	if (v < min)
		v += range * roundf((min - v) / range + 1);

	return min + fmodf((v - min), range);
}

int64_t math_wrapS(int64_t v, int64_t min, int64_t max) {
	int range = max - min;

	if (v < min)
		v += range * ((min - v) / range + 1);

	return min + (v - min) % range;
}

f64 math_pingpongF(f64 v, f64 min, f64 max) {
	min = math_wrapF(v, min, max * 2);
	if (min < max)
		return min;
	else
		return 2 * max - min;
}

int64_t math_pingpongS(int64_t v, int64_t min, int64_t max) {
	min = math_wrapS(v, min, max * 2);
	if (min < max)
		return min;
	else
		return 2 * max - min;
}

/////////////////////////////////////////////////////////////////////////////

f64 math_align_min(f64 v, f64 inv_div) {
	return floor(v / inv_div) * inv_div;
}

/////////////////////////////////////////////////////////////////////////////
