#include <ccmath.h>

// Levinson-Durbin algorithm for iteratively solving for prediction coefficients
static int durbin(f64* acvec, int order, f64* reflection_coeffs, f64* prediction_coeffs, f64* error) {
	int i, j;
	f64 sum, E;
	int ret;

	prediction_coeffs[0] = 1.0;
	E = acvec[0]; // E[0] = r{xx}[0]
	ret = 0;

	for (i = 1; i <= order; i++) {
        // SUM(j, a[i-1][j] * r{xx}[i - j] )
		sum = 0.0;
		for (j = 1; j <= i - 1; j++) {
			sum += prediction_coeffs[j] * acvec[i - j];
		}

        // a[i][i] = -Delta[i-1] / E[i-1]
		prediction_coeffs[i] = (E > 0.0) ? (-(acvec[i] + sum) / E) : 0.0;
        // k[i] = a[i][i]
		reflection_coeffs[i] = prediction_coeffs[i];

		if (math_absF(reflection_coeffs[i]) > 1.0) {
            // incr when a predictor coefficient is > 1 (indicates numerical instability)
			ret++;
		}

		for (j = 1; j < i; j++) {
            // a[i][j] = a[i-1][j] + a[i-1][i - j] * a[i][i]
			prediction_coeffs[j] += prediction_coeffs[i - j] * prediction_coeffs[i];
		}

        // E[i] = E[i-1] * (1.0 - k[i] ** 2)
        //      = E[i-1] * (1.0 - a[i][i] ** 2)
		E *= 1.0 - prediction_coeffs[i] * prediction_coeffs[i];
	}
	*error = E;
	return ret;
}

// Reflection coefficients (k) -> Predictor coefficients (a)
// A subset of Levinson-Durbin that only computes predictors from known reflection coefficients and previous predictors
static void afromk(f64* k, f64* ai, int order) {
	int i, j;

	ai[0] = 1.0;

	for (i = 1; i <= order; i++) {
        // a[i][i] = k[i]
		ai[i] = k[i];

		for (j = 1; j <= i - 1; j++) {
            // a[i][j] = a[i-1][j] + a[i-1][i - j] * k[i] = a[i-1][j] + a[i-1][i - j] * a[i][i]
			ai[j] += ai[i - j] * ai[i];
		}
	}
}

// Prediction coefficients (a) -> Reflection coefficients (k)
// Performs afromk in reverse?
// Returns 0 if numerically stable, otherwise non-zero
static int kfroma(f64* in, f64* out, int order) {
	int i, j;
	f64 div;
	f64 temp;
	f64* next = malloc((order + 1) * sizeof(f64));
	int ret = 0;

	out[order] = in[order];

	for (i = order - 1; i >= 1; i--) {
		for (j = 0; j <= i; j++) {
			temp = out[i + 1];
			div = 1.0 - temp * temp;
			if (div == 0.0) {
				free(next);
				return 1;
			}
			next[j] = (in[j] - in[i - j + 1] * temp) / div;
		}

		for (j = 0; j <= i; j++) {
			in[j] = next[j];
		}

		out[i] = next[i];
		if (math_absF(out[i]) > 1.0) {
            // Not numerically stable
			ret++;
		}
	}

	free(next);
	return ret;
}

// r{xx} from a ?
static void rfroma(f64* in, int n, f64* out) {
	int i, j;
	f64** mat;
	f64 div;

	mat = malloc((n + 1) * sizeof(f64*));
	mat[n] = malloc((n + 1) * sizeof(f64));
	mat[n][0] = 1.0;
	for (i = 1; i <= n; i++) {
		mat[n][i] = -in[i];
	}

	for (i = n; i >= 1; i--) {
		mat[i - 1] = malloc(i * sizeof(f64));
		div = 1.0 - mat[i][i] * mat[i][i];
		for (j = 1; j <= i - 1; j++) {
			mat[i - 1][j] = (mat[i][i - j] * mat[i][i] + mat[i][j]) / div;
		}
	}

	out[0] = 1.0;
	for (i = 1; i <= n; i++) {
		out[i] = 0.0;
		for (j = 1; j <= i; j++) {
			out[i] += mat[i][j] * out[i - j];
		}
	}

	free(mat[n]);
	for (i = n; i > 0; i--) {
		free(mat[i - 1]);
	}
	free(mat);
}

static f64 model_dist(f64* predictors, f64* data, int order) {
	f64 autocorrelation_data[order + 1];
	f64 autocorrelation_predictors[order + 1];
	f64 ret;
	int i, j;

    // autocorrelation from data
	rfroma(data, order, autocorrelation_data);

    // autocorrelation from predictors
	for (i = 0; i <= order; i++) {
		autocorrelation_predictors[i] = 0.0;
		for (j = 0; j <= order - i; j++) {
			autocorrelation_predictors[i] += predictors[j] * predictors[i + j];
		}
	}

    // compute model distance (2 * inner(ac1, ac2) )
	ret = autocorrelation_data[0] * autocorrelation_predictors[0];
	for (i = 1; i <= order; i++) {
		ret += 2 * autocorrelation_data[i] * autocorrelation_predictors[i];
	}

	return ret;
}

// Calculate the autocorrelation matrix of two vectors at x and x - xlen
static void acmat(int16_t* x, int order, int xlen, f64** ac) {
	int i, j, k;

	for (i = 1; i <= order; i++) {
		for (j = 1; j <= order; j++) {
            // R{xx}[i,j] = E[X[i] * X[j]]

			ac[i][j] = 0.0;
			for (k = 0; k < xlen; k++) {
				ac[i][j] += x[k - i] * x[k - j];
			}
		}
	}
}

// Computes the autocorrelation vector of two vectors at x and x - xlen
static void acvect(int16_t* x, int order, int xlen, f64* ac) {
	int i, j;

	for (i = 0; i <= order; i++) {
		ac[i] = 0.0;
        // r{xx} = E(x(m)x) = SUM(j, x[j - i] * x[j])
		for (j = 0; j < xlen; j++) {
			ac[i] -= x[j - i] * x[j];
		}
	}
}

// Lower-Upper (with Permutation vector) Decomposition
// returns 0 if numerically stable
static int lud(f64** a, int n, int* perm, int* d) {
	int i, imax = 0, j, k;
	f64 big, dum, sum, temp;
	f64 min, max;
	f64 vv[n + 1];

	*d = 1;
	for (i = 1; i<=n; i++) {
		big = 0.0;
		for (j = 1; j<=n; j++)
			if ((temp = math_absF(a[i][j])) > big)
                big = temp;

		if (big == 0.0)
            return 1;

		vv[i] = 1.0 / big;
	}

	for (j = 1; j<=n; j++) {
		for (i = 1; i<j; i++) {
			sum = a[i][j];
			for (k = 1; k<i; k++)
                sum -= a[i][k] * a[k][j];

			a[i][j] = sum;
		}

		big = 0.0;

		for (i = j; i<=n; i++) {
			sum = a[i][j];
			for (k = 1; k<j; k++)
				sum -= a[i][k] * a[k][j];

			a[i][j] = sum;

			if ((dum = vv[i] * math_absF(sum)) >= big) {
				big = dum;
				imax = i;
			}
		}

		if (j != imax) {
			for (k = 1; k<=n; k++) {
				dum = a[imax][k];
				a[imax][k] = a[j][k];
				a[j][k] = dum;
			}

			*d = -(*d);
			vv[imax] = vv[j];
		}

		perm[j] = imax;

		if (a[j][j] == 0.0)
            return 1;

		if (j != n) {
			dum = 1.0 / (a[j][j]);
			for (i = j + 1; i<=n; i++)
                a[i][j] *= dum;
		}
	}

	min = 1e10;
	max = 0.0;
	for (i = 1; i <= n; i++) {
		temp = math_absF(a[i][i]);
		if (temp < min)
            min = temp;
		if (temp > max)
            max = temp;
	}
	return (min / max < 1e-10) ? 1 : 0;
}

// LU Backsubstitution
static void lubksb(f64** a, int n, int* perm, f64* b) {
	int i, ii = 0, ip, j;
	f64 sum;

	for (i = 1; i<=n; i++) {
		ip = perm[i];
		sum = b[ip];
		b[ip] = b[i];

		if (ii) {
			for (j = ii; j<=i - 1; j++)
                sum -= a[i][j] * b[j];
        } else if (sum) {
            ii = i;
        }

		b[i] = sum;
	}

	for (i = n; i>=1; i--) {
		sum = b[i];
		for (j = i + 1; j<=n; j++)
            sum -= a[i][j] * b[j];
		b[i] = sum / a[i][i];
	}
}

static void split(f64** predictors, f64* delta, int order, int npredictors, f64 scale) {
	int i, j;

	for (i = 0; i < npredictors; i++) {
		for (j = 0; j <= order; j++) {
			predictors[i + npredictors][j] = predictors[i][j] + delta[j] * scale;
		}
	}
}

static void refine(f64** predictors, int order, int npredictors, f64** data, int dataSize, int refineIters) {
	int iter;
	f64** rsums;
	int* counts;
	f64* vec;
	f64 dist;
	f64 dummy;
	f64 bestValue;
	int bestIndex;
	int i, j;

	rsums = malloc(npredictors * sizeof(f64*));
	for (i = 0; i < npredictors; i++) {
		rsums[i] = malloc((order + 1) * sizeof(f64));
	}

	counts = malloc(npredictors * sizeof(int));
	vec = malloc((order + 1) * sizeof(f64));

	for (iter = 0; iter < refineIters; iter++) {
        // For some number of refinement iterations

        // Initialize averages
		for (i = 0; i < npredictors; i++) {
			counts[i] = 0;
			for (j = 0; j <= order; j++) {
				rsums[i][j] = 0.0;
			}
		}

        // Sum autocorrelations
		for (i = 0; i < dataSize; i++) {
			bestValue = 1e30;
			bestIndex = 0;

            // Find index that minimizes model distance
			for (j = 0; j < npredictors; j++) {
				dist = model_dist(predictors[j], data[i], order);
				if (dist < bestValue) {
					bestValue = dist;
					bestIndex = j;
				}
			}

			counts[bestIndex]++;
			rfroma(data[i], order, vec); // compute autocorrelation from predictors
			for (j = 0; j <= order; j++) {
				rsums[bestIndex][j] += vec[j]; // add to average autocorrelation
			}
		}

        // finalize average autocorrelations
		for (i = 0; i < npredictors; i++) {
			if (counts[i] > 0) {
				for (j = 0; j <= order; j++) {
					rsums[i][j] /= counts[i];
				}
			}
		}

		for (i = 0; i < npredictors; i++) {
            // compute predictors from average autocorrelation
			durbin(rsums[i], order, vec, predictors[i], &dummy);
            // vec is reflection coeffs

            // clamp reflection coeffs
			for (j = 1; j <= order; j++) {
				if (vec[j] >=  1.0)
                    vec[j] =  0.9999999999;
				if (vec[j] <= -1.0)
                    vec[j] = -0.9999999999;
			}

            // clamped reflection coeffs -> predictors
			afromk(vec, predictors[i], order);
		}
	}

	free(counts);
	for (i = 0; i < npredictors; i++) {
		free(rsums[i]);
	}
	free(rsums);
	free(vec);
}

#include "z64sample.h"
#include "vadpcm.h"
#include <ccmath.h>

static int read_row(int16_t* p, f64* row, int order) {
	double fval;
	int ival;
	int i, j, k;
	int overflows;
	double table[8][order];

	for (i = 0; i < order; i++) {
		for (j = 0; j < i; j++)
			table[i][j] = 0.0;

		for (j = i; j < order; j++)
			table[i][j] = -row[order - j + i];
	}

	for (i = order; i < 8; i++)
		for (j = 0; j < order; j++)
			table[i][j] = 0.0;

	for (i = 1; i < 8; i++)
		for (j = 1; j <= order; j++)
			if (i - j >= 0)
				for (k = 0; k < order; k++)
					table[i][k] -= row[j] * table[i - j][k];

	overflows = 0;

	for (i = 0; i < order; i++) {
		for (j = 0; j < 8; j++) {
			fval = table[j][i] * 2048.0;
			if (fval < 0.0) {
				ival = (int) (fval - 0.5);
				if (ival < -0x8000)
					overflows++;
			} else {
				ival = (int) (fval + 0.5);
				if (ival >= 0x8000)
					overflows++;
			}

			printerrnnl("%04X ", (uint16_t)ival);
			*(p++) = swapBE(*ptrval(uint16_t, ival));
		}
		printerr("");
	}

	return overflows;
}

static z64TableDesign s_default_design = {
	.refine_iters = 2,
	.order        = 2,
	.bits         = 2,
	.thresh       = 10.0,
};

void z64sample_tabledesign(z64sample* this, z64TableDesign* design) {
	if (!design) design = &s_default_design;

	int frame_size = 16;
	int num_order = design->order + 1;

	uint32_t data_size = 0;
	uint32_t sample_num = this->sample_num;

	int32_t* perm =    new(int32_t[num_order]);
	int16_t* buffer = new(int16_t[frame_size * 2]);
	f64* vec =         new(f64[num_order]);
	f64* reflection_coeffs = new(f64[num_order]);
	f64** data =       new(f64*[sample_num]);
	f64** autocorrelation_matrix =        new(f64*[num_order]);
	f64** predictors =    new(f64*[1 << design->bits]);

	for (int i = 0; i < (1 << design->bits); i++)
		predictors[i] =   new(f64[num_order]);
	for (int i = 0; i < num_order; i++)
		autocorrelation_matrix[i] =       new(f64[num_order]);

	int16_t* sample = this->samples_l;
	int16_t* sample_end = sample + (uint32_t)math_align_min(sample_num, 16.0);

	print_align("bits:", "%d", design->bits);
	print_align("order:", "%d", design->order);
	print_align("refine iters:", "%d", design->refine_iters);
	print_align("thresh:", "%g", design->thresh);

	for (; sample < sample_end; sample += frame_size) {
        // Copy sample data into second half of buffer, during the first iteration the first half is 0 while in
        // later iterations the second half of the previous iteration is shifted into the first half.
		memcpy(&buffer[frame_size], sample, sizeof(int16_t[frame_size]));

        // Compute autocorrelation vector of the two vectors in the buffer
		acvect(&buffer[frame_size], design->order, frame_size, vec);

        // First element is the largest(?)
		if (math_absF(vec[0]) > design->thresh) {
            // Over threshold

			int dummy;

            // Computes the autocorrelation matrix of the two vectors in the buffer
			acmat(&buffer[frame_size], design->order, frame_size, autocorrelation_matrix);

            // Compute the LUP decomposition of the autocorrelation matrix
			if (lud(autocorrelation_matrix, design->order, perm, &dummy) == 0) { // Continue only if numerically stable
                // Back-substitution to solve the linear equation Ra = r
                // where
                //  R = autocorrelation matrix
                //  r = autocorrelation vector
                //  a = linear prediction coefficients
                // After this vec contains the prediction coefficients
				lubksb(autocorrelation_matrix, design->order, perm, vec);
				vec[0] = 1.0;

                // Compute reflection coefficients from prediction coefficients
				if (kfroma(vec, reflection_coeffs, design->order) == 0) { // Continue only if numerically stable
					data[data_size] = new(f64[num_order]);
					data[data_size][0] = 1.0;

                    // clamp the reflection coefficients
					for (int i = 1; i < num_order; i++) {
						if (reflection_coeffs[i] >=  1.0)
                            reflection_coeffs[i] =  0.9999999999;
						if (reflection_coeffs[i] <= -1.0)
                            reflection_coeffs[i] = -0.9999999999;
					}

                    // Compute prediction coefficients from reflection coefficients
					afromk(reflection_coeffs, data[data_size++], design->order);
				}
			}
		}

        // Move second vector to first vector
		memcpy(&buffer[0], &buffer[frame_size], sizeof(int16_t[frame_size]));
	}

    // Create a vector [1.0, 0.0, ..., 0.0]
    vec[0] = 1.0;
	for (int i = 1; i < num_order; i++)
		vec[i] = 0.0;

	for (uint32_t i = 0; i < data_size; i++) {
        // Compute autocorrelation from predictors
		rfroma(data[i], design->order, predictors[0]);

		for (int k = 1; k < num_order; k++)
			vec[k] += predictors[0][k];
	}

	for (int i = 1; i < num_order; i++)
		vec[i] /= data_size;

    // vec is the average autocorrelation

    // Compute predictors for average autocorrelation using Levinson-Durbin algorithm
	double dummy;
	durbin(vec, design->order, reflection_coeffs, predictors[0], &dummy);

    // clamp results
	for (int i = 0; i < num_order; i++) {
		if (reflection_coeffs[i] >=  1.0)
            reflection_coeffs[i] =  0.9999999999;
		if (reflection_coeffs[i] <= -1.0)
            reflection_coeffs[i] = -0.9999999999;
	}

    // Convert clamped reflection coefficients to predictors
	afromk(reflection_coeffs, predictors[0], design->order);

    // Split and refine predictors
	for (int cur_bits = 0; cur_bits < design->bits; cur_bits++) {
		f64 split_delta[num_order];

		for (int i = 0; i < num_order; i++)
			split_delta[i] = 0.0;
		split_delta[design->order - 1] = -1.0;

		split(predictors, split_delta, design->order, 1 << cur_bits, 0.01);
		refine(predictors, design->order, 1 << (1 + cur_bits), data, data_size, design->refine_iters);
	}

	int16_t order = design->order;
	int16_t npred = 1 << design->bits;

	int16_t* temp_book = new(int16_t[(order * npred * 8) + 2]);
	int16_t* p = temp_book + 2;

	temp_book[0] = swapBE(order);
	temp_book[1] = swapBE(npred);

	print("%d / %d", order, npred);

	int num_oflow = 0;
	for (int i = 0; i < npred; i++)
		num_oflow += read_row(&p[(order * 8) * i], predictors[i], design->order);

	if (num_oflow)
		printerr("overflow!");

	z64sample_read_codebook(this, temp_book);
	delete(temp_book);

	for (uint32_t i = 0; i < sample_num; i++)
		delete(data[i]);
	for (int i = 0; i < num_order; i++)
		delete(autocorrelation_matrix[i]);
	for (int i = 0; i < (1 << design->bits); i++)
		delete(predictors[i]);

	delete(data, autocorrelation_matrix, perm, predictors);
}
