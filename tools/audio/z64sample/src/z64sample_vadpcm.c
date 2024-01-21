#include <cclib.h>
#include <ccmath.h>
#include "z64sample.h"
#include "vadpcm.h"

static int32_t myrand() {
	static uint64_t state = 1619236481962341ULL;

	state *= 3123692312237ULL;
	state += 1;
	return state >> 33;
}

static int16_t qsample(f32 x, int32_t scale) {
	if (x > 0.0f) {
		return (int16_t) ((x / scale) + 0.4999999);
	} else {
		return (int16_t) ((x / scale) - 0.4999999);
	}
}

static void clamp_to_s16(f32* in, int32_t* out) {
	f32 llevel = -0x8000;
	f32 ulevel = 0x7fff;

	for (int32_t i = 0; i < 16; i++) {
		if (in[i] > ulevel)
            in[i] = ulevel;
		if (in[i] < llevel)
            in[i] = llevel;

		if (in[i] > 0.0f) {
			out[i] = (int32_t) (in[i] + 0.5);
		} else {
			out[i] = (int32_t) (in[i] - 0.5);
		}
	}
}

static int16_t clamp_bits(int32_t x, int32_t bits) {
	int32_t lim = 1 << (bits - 1);

	if (x < -lim)
        return -lim;
	if (x > lim - 1)
        return lim - 1;
	return x;
}

static int32_t inner_product(int32_t length, int32_t* v1, int32_t* v2) {
	int32_t out = 0;

	for (int32_t i = 0; i < length; i++) {
		out += v1[i] * v2[i];
	}

	// Compute "out / 2^11", rounded down.
	int32_t dout = out / (1 << 11);
	int32_t fiout = dout * (1 << 11);
	return dout - (out - fiout < 0);
}

static void permute(int32_t* out, int32_t* in, int32_t* decompressed, int32_t scale, int32_t frame_size) {
	int normal = myrand() % 3 == 0;

	for (int32_t i = 0; i < 16; i++) {
		int32_t lo = in[i] - scale / 2;
		int32_t hi = in[i] + scale / 2;
		if (frame_size == 9) {
			if (decompressed[i] == -8 && myrand() % 10 == 0)
                lo -= scale * 3 / 2;
			else if (decompressed[i] == 7 && myrand() % 10 == 0)
                hi += scale * 3 / 2;
		} else if (decompressed[i] == -2 && myrand() % 7 == 0)
            lo -= scale * 3 / 2;
		else if (decompressed[i] == 1 && myrand() % 10 == 0)
            hi += scale * 3 / 2;
		else if (normal) {
            //
		} else if (decompressed[i] == 0) {
			if (myrand() % 3) {
				lo = in[i] - scale / 5;
				hi = in[i] + scale / 5;
			} else if (myrand() % 2) {
				lo = in[i] - scale / 3;
				hi = in[i] + scale / 3;
			}
		} else if (myrand() % 3) {
			if (decompressed[i] < 0)
                lo = in[i] + scale / 4;
			if (decompressed[i] > 0)
                hi = in[i] - scale / 4;
		} else if (myrand() % 2) {
			if (decompressed[i] < 0)
                lo = in[i] - scale / 4;
			if (decompressed[i] > 0)
                hi = in[i] + scale / 4;
		}
		out[i] = clamp_bits(lo + myrand() % (hi - lo + 1), 16);
	}
}

static void get_bounds(int32_t* in, int32_t* decompressed, int32_t scale, int32_t* min_vals, int32_t* max_vals, int32_t frame_size) {
	int32_t minv, maxv;

	if (frame_size == 9) {
		minv = -8;
		maxv = 7;
	} else {
		minv = -2;
		maxv = 1;
	}
	for (int32_t i = 0; i < 16; i++) {
		int32_t lo = in[i] - scale - scale / 2;
		int32_t hi = in[i] + scale + scale / 2;
		if (decompressed[i] == minv)
            lo -= scale;
		else if (decompressed[i] == maxv)
            hi += scale;
		min_vals[i] = lo;
		max_vals[i] = hi;
	}
}

static int64_t scored_encode(int32_t* inbf, int32_t* origState, CoefTable coef_tbl, int32_t order, int32_t num_predictors,
                             int32_t w_predictor, int32_t w_scale, int32_t w_idx[16], int32_t frame_size) {
	int32_t prediction[16];
	int32_t in_vec[16];
	int32_t optimalp = 0;
	int32_t scale;
	int32_t encBits = (frame_size == 5) ? 2 : 4;
	int32_t llevel = -(1 << (encBits - 1));
	int32_t ulevel = -llevel - 1;
	int32_t ie[16];
	f32 e[16];
	f32 min = 1e30;
	int32_t scaleFactor = 16 - encBits;
	f32 errs[4];
	int64_t scoreA = 0, scoreB = 0, scoreC = 0;

	for (int32_t k = 0; k < num_predictors; k++) {
		for (int32_t j = 0; j < 2; j++) {
			for (int32_t i = 0; i < order; i++) {
				in_vec[i] = (j == 0) ? origState[16 - order + i] : inbf[8 - order + i];
			}

			for (int32_t i = 0; i < 8; i++) {
				prediction[j * 8 + i] = inner_product(order + i, coef_tbl[k][i], in_vec);
				in_vec[i + order] = inbf[j * 8 + i] - prediction[j * 8 + i];
				e[j * 8 + i] = (f32) in_vec[i + order];
			}
		}

		f32 se = 0.0f;
		for (int32_t j = 0; j < 16; j++) {
			se += e[j] * e[j];
		}

		errs[k] = se;

		if (se < min) {
			min = se;
			optimalp = k;
		}
	}

	for (int32_t k = 0; k < num_predictors; k++) {
		if (errs[k] < errs[w_predictor]) {
			scoreA += (int64_t)(errs[w_predictor] - errs[k]);
		}
	}
	if (optimalp != w_predictor) {
		// probably penalized above, but add extra penalty in case the error
		// values were the exact same
		scoreA += 1;
	}
	optimalp = w_predictor;

	for (int32_t j = 0; j < 2; j++) {
		for (int32_t i = 0; i < order; i++) {
			in_vec[i] = (j == 0) ? origState[16 - order + i] : inbf[8 - order + i];
		}

		for (int32_t i = 0; i < 8; i++) {
			prediction[j * 8 + i] = inner_product(order + i, coef_tbl[optimalp][i], in_vec);
			e[j * 8 + i] = in_vec[i + order] = inbf[j * 8 + i] - prediction[j * 8 + i];
		}
	}

	clamp_to_s16(e, ie);

	int32_t max = 0;
	for (int32_t i = 0; i < 16; i++) {
		if (abs(ie[i]) > abs(max)) {
			max = ie[i];
		}
	}

	for (scale = 0; scale <= scaleFactor; scale++) {
		if (max <= ulevel && max >= llevel)
            break;
		max /= 2;
	}

	// Preliminary ix computation, computes whether scale needs to be incremented
	int32_t state[16];
	bool again = false;

    for (int32_t j = 0; j < 2; j++) {
		int32_t base = j * 8;

		for (int32_t i = 0; i < order; i++) {
			in_vec[i] = (j == 0) ? origState[16 - order + i] : state[8 - order + i];
		}

		for (int32_t i = 0; i < 8; i++) {
			prediction[base + i] = inner_product(order + i, coef_tbl[optimalp][i], in_vec);

			f32 se = (f32) inbf[base + i] - (f32) prediction[base + i];
			int32_t ix = qsample(se, 1 << scale);

			int32_t clampedIx = clamp_bits(ix, encBits);
			int32_t cV = clampedIx - ix;
			if (cV > 1 || cV < -1) {
				again = true;
			}

			in_vec[i + order] = clampedIx * (1 << scale);
			state[base + i] = prediction[base + i] + in_vec[i + order];
		}
	}

	if (again && scale < scaleFactor) {
		scale++;
	}

	if (scale != w_scale) {
		// We could do math to penalize scale mismatches accurately, but it's
		// simpler to leave it as a constraint by setting an infinite penalty.
		scoreB += 100000000;
		scale = w_scale;
	}

	// Then again for real, but now also with penalty computation
	for (int32_t j = 0; j < 2; j++) {
		int32_t base = j * 8;

		for (int32_t i = 0; i < order; i++) {
			in_vec[i] = (j == 0) ? origState[16 - order + i] : state[8 - order + i];
		}

		for (int32_t i = 0; i < 8; i++) {
			prediction[base + i] = inner_product(order + i, coef_tbl[optimalp][i], in_vec);

			int64_t ise = (int64_t) inbf[base + i] - (int64_t) prediction[base + i];
			f32 se = (f32) inbf[base + i] - (f32) prediction[base + i];
			int32_t ix = qsample(se, 1 << scale);

			int32_t clampedIx = clamp_bits(ix, encBits);
			int32_t val = w_idx[base + i] * (1 << scale);
			if (clampedIx != w_idx[base + i]) {
				if (ix == w_idx[base + i])
					throw(ERR_ERROR, "bad things!");

				int32_t lo = val - (1 << scale) / 2;
				int32_t hi = val + (1 << scale) / 2;

				int64_t diff = 0;
				if (ise < lo)
                    diff = lo - ise;
				else if (ise > hi)
                    diff = ise - hi;

				scoreC += diff * diff + 1;
			}

			in_vec[i + order] = val;
			state[base + i] = prediction[base + i] + val;
		}
	}

	// Penalties for going outside int16_t
	for (int32_t i = 0; i < 16; i++) {
		int64_t diff = 0;
		if (inbf[i] < -0x8000)
            diff = -0x8000 - inbf[i];
		if (inbf[i] > 0x7fff)
            diff = inbf[i] - 0x7fff;
		scoreC += diff * diff;
	}

	return scoreA + scoreB + 10 * scoreC;
}

static int32_t descent(int32_t guess[16], int32_t min_vals[16], int32_t max_vals[16], int32_t prev_state[16],
                       CoefTable coef_tbl, int32_t order, int32_t num_predictors, int32_t w_predictor, int32_t w_scale,
                       int32_t w_idx[32], int32_t frame_size) {
	const f64 inf = 1e100;
	int64_t curScore = scored_encode(guess, prev_state, coef_tbl, order, num_predictors, w_predictor, w_scale, w_idx,
                                     frame_size);

	while (true) {
		f64 delta[16];

		if (curScore == 0) {
			return 1;
		}

		// Compute gradient, and how far to move along it at most.
		f64 maxMove = inf;
		for (int32_t i = 0; i < 16; i++) {
			guess[i] += 1;
			int64_t scoreUp = scored_encode(guess, prev_state, coef_tbl, order, num_predictors, w_predictor, w_scale,
                                            w_idx, frame_size);
			guess[i] -= 2;
			int64_t scoreDown = scored_encode(guess, prev_state, coef_tbl, order, num_predictors, w_predictor, w_scale,
                                              w_idx, frame_size);
			guess[i] += 1;

			if (scoreUp >= curScore && scoreDown >= curScore) {
				// Don't touch this coordinate
				delta[i] = 0;
			} else if (scoreDown < scoreUp) {
				if (guess[i] == min_vals[i]) {
					// Don't allow moving out of bounds
					delta[i] = 0;
				} else {
					delta[i] = -(f64)(curScore - scoreDown);
					maxMove = math_minF(maxMove, (min_vals[i] - guess[i]) / delta[i]);
				}
			} else {
				if (guess[i] == max_vals[i]) {
					delta[i] = 0;
				} else {
					delta[i] = (f64)(curScore - scoreUp);
					maxMove = math_minF(maxMove, (max_vals[i] - guess[i]) / delta[i]);
				}
			}
		}
		if (maxMove == inf || maxMove <= 0) {
			return 0;
		}

		// Try exponentially spaced candidates along the gradient.
		int32_t nguess[16];
		int32_t bestGuess[16];
		int64_t bestScore = curScore;
		while (true) {
			bool changed = false;

			for (int32_t i = 0; i < 16; i++) {
				nguess[i] = (int32_t)math_roundF(guess[i] + delta[i] * maxMove);
				if (nguess[i] != guess[i])
                    changed = true;
			}
			if (!changed)
                break;

			int64_t score = scored_encode(nguess, prev_state, coef_tbl, order, num_predictors, w_predictor, w_scale,
                                          w_idx, frame_size);
			if (score < bestScore) {
				bestScore = score;
				memcpy(bestGuess, nguess, sizeof(nguess));
			}
			maxMove *= 0.7;
		}

		if (bestScore == curScore) {
			// No improvements along that line, give up.
			return 0;
		}
		curScore = bestScore;
		memcpy(guess, bestGuess, sizeof(bestGuess));
	}
}

///////////////////////////////////////////////////////////////////////////////

static void vadpcm_decode(uint8_t* frame, int32_t* decompressed, int32_t* state, int32_t order, CoefTable coef_tbl,
                          int32_t frame_size) {
	int32_t ix[16];

	uint8_t header = frame[0];
	int32_t scale = 1 << (header >> 4);
	int32_t optimalp = header & 0xf;

    // Unpack frame
	if (frame_size == 5) {
		for (int32_t i = 0; i < 16; i += 4) {
			uint8_t c = frame[1 + i / 4];
			ix[i + 0] = (c >> 6) & 0b11;
			ix[i + 1] = (c >> 4) & 0b11;
			ix[i + 2] = (c >> 2) & 0b11;
			ix[i + 3] = (c >> 0) & 0b11;
		}
	} else { // 9
		for (int32_t i = 0; i < 16; i += 2) {
			uint8_t c = frame[1 + i / 2];
			ix[i + 0] = (c >> 4) & 0xF;
			ix[i + 1] = (c >> 0) & 0xF;
		}
	}

    // Sign extend, multiply by scale
	for (int32_t i = 0; i < 16; i++) {
		if (frame_size == 5) {
			if (ix[i] >= 2) // 2-bit sign extension
                ix[i] -= 4;
		} else { // 9
			if (ix[i] >= 8) // 4-bit sign extension
                ix[i] -= 16;
		}
		decompressed[i] = ix[i]; // save decompressed frame data before scaling
		ix[i] *= scale;
	}

	for (int32_t j = 0; j < 2; j++) {
		int32_t in_vec[16];

		if (j == 0) {
			for (int32_t i = 0; i < order; i++) {
				in_vec[i] = state[16 - order + i];
			}
		} else {
			for (int32_t i = 0; i < order; i++) {
				in_vec[i] = state[8 - order + i];
			}
		}

        // IIR filter
		for (int32_t i = 0; i < 8; i++) {
			int32_t ind = j * 8 + i;
			in_vec[order + i] = ix[ind];
			state[ind] = inner_product(order + i, coef_tbl[optimalp][i], in_vec) + ix[ind];
		}
	}
}

static void vadpcm_encode_for_brute(uint8_t* out, int16_t* inbf, int32_t* origState, CoefTable coef_tbl, int32_t order,
                                    int32_t num_predictors, int32_t frame_size) {
	int16_t ix[16];
	int32_t prediction[16];
	int32_t in_vec[16];
	int32_t optimalp = 0;
	int32_t scale;
	int32_t ie[16];
	f32 e[16];
	f32 min = 1e30;

	for (int32_t k = 0; k < num_predictors; k++) {
		for (int32_t j = 0; j < 2; j++) {
			for (int32_t i = 0; i < order; i++) {
				in_vec[i] = (j == 0 ? origState[16 - order + i] : inbf[8 - order + i]);
			}

			for (int32_t i = 0; i < 8; i++) {
				prediction[j * 8 + i] = inner_product(order + i, coef_tbl[k][i], in_vec);
				in_vec[i + order] = inbf[j * 8 + i] - prediction[j * 8 + i];
				e[j * 8 + i] = (f32) in_vec[i + order];
			}
		}

		f32 se = 0.0f;
		for (int32_t j = 0; j < 16; j++) {
			se += e[j] * e[j];
		}

		if (se < min) {
			min = se;
			optimalp = k;
		}
	}

	for (int32_t j = 0; j < 2; j++) {
		for (int32_t i = 0; i < order; i++) {
			in_vec[i] = (j == 0 ? origState[16 - order + i] : inbf[8 - order + i]);
		}

		for (int32_t i = 0; i < 8; i++) {
			prediction[j * 8 + i] = inner_product(order + i, coef_tbl[optimalp][i], in_vec);
			e[j * 8 + i] = in_vec[i + order] = inbf[j * 8 + i] - prediction[j * 8 + i];
		}
	}

	clamp_to_s16(e, ie);

	int32_t max = 0;
	for (int32_t i = 0; i < 16; i++) {
		if (abs(ie[i]) > abs(max)) {
			max = ie[i];
		}
	}

	int32_t encBits = (frame_size == 5 ? 2 : 4);
	int32_t llevel = -(1 << (encBits - 1));
	int32_t ulevel = -llevel - 1;
	int32_t scaleFactor = 16 - encBits;

	for (scale = 0; scale <= scaleFactor; scale++) {
		if (max <= ulevel && max >= llevel)
            break;
		max /= 2;
	}

	int32_t state[16];
	for (int32_t i = 0; i < 16; i++) {
		state[i] = origState[i];
	}

    bool again = true;
	for (int32_t nIter = 0; nIter < 2 && again; nIter++) {
		again = false;

		if (nIter == 1)
            scale++;
		if (scale > scaleFactor) {
			scale = scaleFactor;
		}

		for (int32_t j = 0; j < 2; j++) {
			int32_t base = j * 8;

			for (int32_t i = 0; i < order; i++) {
				in_vec[i] = (j == 0 ?
					origState[16 - order + i] : state[8 - order + i]);
			}

			for (int32_t i = 0; i < 8; i++) {
				prediction[base + i] = inner_product(order + i, coef_tbl[optimalp][i], in_vec);

				f32 se = (f32) inbf[base + i] - (f32) prediction[base + i];
				ix[base + i] = qsample(se, 1 << scale);

				int32_t cV = clamp_bits(ix[base + i], encBits) - ix[base + i];
				if (cV > 1 || cV < -1)
                    again = true;
				ix[base + i] += cV;

				in_vec[i + order] = ix[base + i] * (1 << scale);
				state[base + i] = prediction[base + i] + in_vec[i + order];
			}
		}
	}

	out[0] = (scale << 4) | (optimalp & 0xf);
	if (frame_size == 5) {
		for (int32_t i = 0; i < 16; i += 4) {
			uint8_t c = ((ix[i] & 0x3) << 6) | ((ix[i + 1] & 0x3) << 4) | ((ix[i + 2] & 0x3) << 2) | (ix[i + 3] & 0x3);
			out[1 + i / 4] = c;
		}
	} else {
		for (int32_t i = 0; i < 16; i += 2) {
			uint8_t c = ((ix[i] & 0xf) << 4) | (ix[i + 1] & 0xf);
			out[1 + i / 2] = c;
		}
	}
}

static int32_t vadpcm_brute(int32_t guess[16], uint8_t input[9], int32_t decoded[16], int32_t decompressed[16],
                            int32_t prev_state[16], CoefTable coef_tbl, int32_t order, int32_t num_predictors,
                            int32_t frame_size) {
	int32_t min_vals[16], max_vals[16];

	int32_t scale = input[0] >> 4;
    int32_t predictor = input[0] & 0xF;

	get_bounds(decoded, decompressed, 1 << scale, min_vals, max_vals, frame_size);

	int32_t i = 0;
	while (true) {
		int64_t bestScore = -1;
		int32_t bestGuess[16];

		for (int i = 0; i < 100; i++) {
			permute(guess, decoded, decompressed, 1 << scale, frame_size);
			int64_t score = scored_encode(guess, prev_state, coef_tbl, order, num_predictors, predictor, scale,
                                          decompressed, frame_size);

			if (score == 0)
				return 1;

			if (bestScore == -1 || score < bestScore) {
				bestScore = score;
				memcpy(bestGuess, guess, sizeof(bestGuess));
			}
		}
		memcpy(guess, bestGuess, sizeof(bestGuess));

		if (descent(guess, min_vals, max_vals, prev_state, coef_tbl, order, num_predictors, predictor, scale,
                    decompressed, frame_size))
			return 1;

        i++;
		if (i == __INT32_MAX__ / 10)
			print_exit("giving up on bruteforce!");
	}
}

///////////////////////////////////////////////////////////////////////////////

int z64sample_read_codebook(z64sample* this, void* p) {
	try {
		z64CodebookContext* book_ctx;
		CoefTable book;

		if (!(book_ctx = this->codebook_ctx = new(z64CodebookContext)))
			throw(ERR_ALLOC, "new(z64coeftableCtx)");

		int order = book_ctx->order = swapBE(*readbuf(uint16_t, p));
		int npred = book_ctx->npred = swapBE(*readbuf(uint16_t, p));

		if (!(book_ctx->raw = memdup(p, order * npred * 8 * sizeof(int16_t))))
			throw(ERR_ALLOC, "memdup(p)");

		if (!(book = book_ctx->coef_tbl = new(int32_t * * [npred])))
			throw(ERR_ERROR, "new(int32_t**[%d])", npred);

		for (int i = 0; i < npred; i++) {
			if (!(book[i] = new(int32_t*[8])))
				throw(ERR_ERROR, "new(int32_t*[8])");

			for (int j = 0; j < 8; j++)
				if (!(book[i][j] = new(int32_t[8 + order])))
					throw(ERR_ERROR, "new(int32_t[8 + %d])", order);
		}

		for (int i = 0; i < npred; i++) {

			for (int j = 0; j < order; j++) {
				for (int k = 0; k < 8; k++) {
					book[i][k][j] = (int16_t)swapBE(*readbuf(uint16_t, p));
				}
			}

			for (int k = 1; k < 8; k++)
				book[i][k][order] = book[i][k - 1][order - 1];

			book[i][0][order] = 1 << 11;

			for (int k = 1; k < 8; k++) {
				int j = 0;

				for (; j < k; j++)
					book[i][j][k + order] = 0;

				for (; j < 8; j++)
					book[i][j][k + order] = book[i][j - k][order];
			}
		}

	} except {
		print_exception();
		z64sample_destroy_codebook(this);

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

void z64sample_destroy_codebook(z64sample* this) {
	z64CodebookContext* book_ctx = this->codebook_ctx;

	if (book_ctx) {
		if (book_ctx->coef_tbl) {
			for (int i = 0; i < book_ctx->npred; i++) {
				if (book_ctx->coef_tbl[i])
					for (int j = 0; j < 8; j++)
						delete(book_ctx->coef_tbl[i][j]);
				delete(book_ctx->coef_tbl[i]);
			}
		}
		delete(book_ctx->coef_tbl, book_ctx->raw);
	}
	delete(this->codebook_ctx);
}

///////////////////////////////////////////////////////////////////////////////

void vadpcm_read_frame(z64VadCodecContext* ctx, z64CodebookContext* book, void* src, int frame_size, int8_t matching) {
	uint8_t input[9];
	uint8_t rencoded[9];
	int32_t decoded[16];
	int32_t prev_state[16];
	int16_t orig_guess[16];
	int32_t decompressed[16];

	memcpy(input, src, 9);

	if (!matching) {
        // Non-matching path, fast decode that may not match when re-encoded

		vadpcm_decode(input, decompressed, ctx->state, book->order, book->coef_tbl, frame_size);

		for (int i = 0; i < 16; i++)
			ctx->guess[i] = clamp_bits(ctx->state[i], 16);

		return;
	}

    // Matching path, brute-force decoding so that it matches when re-encoded

	memcpy(prev_state, ctx->state, sizeof(int32_t[16]));

	vadpcm_decode(input, decompressed, ctx->state, book->order, book->coef_tbl, frame_size);
	memcpy(decoded, ctx->state, sizeof(ctx->state));

	for (int i = 0; i < 16; i++)
		orig_guess[i] = clamp_bits(ctx->state[i], 16);

	memcpy(ctx->guess, orig_guess, sizeof(ctx->guess));
	vadpcm_encode_for_brute(rencoded, ctx->guess, prev_state, book->coef_tbl, book->order, book->npred, frame_size);

	if (!memeq(input, rencoded, frame_size)) {
		{
			int32_t guess_32[16];
			vadpcm_brute(guess_32, input, decoded, decompressed, prev_state, book->coef_tbl, book->order, book->npred, frame_size);

			for (int i = 0; i < 16; i++) {
				if (guess_32[i] != (int16_t)guess_32[i])
					printerr("bad frame: " COL_GRAY "0x" COL_RED "%08X" COL_GRAY, 0xDEADBEEF);
				ctx->guess[i] = guess_32[i];
			}

			vadpcm_encode_for_brute(rencoded, ctx->guess, prev_state, book->coef_tbl, book->order, book->npred, frame_size);
			if (!memeq(input, rencoded, frame_size))
				printerr("non-matching: " COL_GRAY "0x" COL_RED "%08X" COL_GRAY, 0xDEADBEEF);
		}

		for (int i = 0; i < 50; i++) {
			int32_t k = myrand() % 16;
			int32_t cur = ctx->guess[k];

			if (cur == orig_guess[k])
                continue;

			ctx->guess[k] = orig_guess[k];
			if (myrand() % 2)
                ctx->guess[k] += (cur - orig_guess[k]) / 2;

			vadpcm_encode_for_brute(rencoded, ctx->guess, prev_state, book->coef_tbl, book->order, book->npred, frame_size);

			if (memeq(input, rencoded, frame_size))
				i = -1;
			else
				ctx->guess[k] = cur;
		}
	}

	memcpy(ctx->state, decoded, sizeof(ctx->state));
}

///////////////////////////////////////////////////////////////////////////////

static void clampv(int32_t fs, f32* e, int32_t* ie, int32_t bits) {
	int32_t i;
	f32 ulevel;
	f32 llevel;

	llevel = -(f32) (1 << (bits - 1));
	ulevel = -llevel - 1.0f;
	for (i = 0; i < fs; i++) {
		if (e[i] > ulevel) {
			e[i] = ulevel;
		}
		if (e[i] < llevel) {
			e[i] = llevel;
		}

		if (e[i] > 0.0f) {
			ie[i] = (int32_t) (e[i] + 0.5);
		} else {
			ie[i] = (int32_t) (e[i] - 0.5);
		}
	}
}

static int32_t clip(int32_t ix, int32_t llevel, int32_t ulevel) {
	if (ix < llevel || ix > ulevel) {
		if (ix < llevel) {
			return llevel;
		}
		if (ix > ulevel) {
			return ulevel;
		}
	}
	return ix;
}

void vadpcm_write_frame(z64VadCodecContext* ctx, z64CodebookContext* book, void* src, int frame_size, int num_sample) {
	int16_t in_buf[16] = {};

	memcpy(in_buf, src, sizeof(int16_t[num_sample]));

	int32_t in_vec[16];
	int32_t prediction[16];
	int32_t optimalp;
	f32 e[16];
	f32 se;
	f32 min = 1e30;
	int32_t i;
	int32_t j;
	int32_t k;

    // Determine the best-fitting predictor.

	optimalp = 0;
	for (k = 0; k < book->npred; k++) {
        // Copy over the last 'order' samples from the previous output.
		for (i = 0; i < book->order; i++)
			in_vec[i] = ctx->state[16 - book->order + i];

        // For 8 samples...
		for (i = 0; i < 8; i++) {
            // Compute a prediction based on 'order' values from the old state,
            // plus previous *quantized* errors in this chunk (because that's
            // all the decoder will have available).
			prediction[i] = inner_product(book->order + i, book->coef_tbl[k][i], in_vec);

            // Record the error in in_vec (thus, its first 'order' samples
            // will contain actual values, the rest will be error terms), and
            // in floating point form in e (for no particularly good reason).
			in_vec[i + book->order] = in_buf[i] - prediction[i];
			e[i] = (f32) in_vec[i + book->order];
		}

        // For the next 8 samples, start with 'order' values from the end of
        // the previous 8-sample chunk of in_buf. (The code is equivalent to
        // inVector[i] = in_buf[8 - order + i].)
		for (i = 0; i < book->order; i++)
			in_vec[i] = prediction[8 - book->order + i] + in_vec[8 + i];

        // ... and do the same thing as before to get predictions.
		for (i = 0; i < 8; i++) {
			prediction[8 + i] = inner_product(book->order + i, book->coef_tbl[k][i], in_vec);
			in_vec[i + book->order] = in_buf[8 + i] - prediction[8 + i];
			e[8 + i] = (f32) in_vec[i + book->order];
		}

        // Compute the L2 norm of the errors; the lowest norm decides which predictor to use.
		se = 0.0f;
		for (j = 0; j < 16; j++)
			se += e[j] * e[j];

		if (se < min) {
			min = se;
			optimalp = k;
		}
	}

    // Do exactly the same thing again, for real.

	for (i = 0; i < book->order; i++)
		in_vec[i] = ctx->state[16 - book->order + i];

	for (i = 0; i < 8; i++) {
		prediction[i] = inner_product(book->order + i, book->coef_tbl[optimalp][i], in_vec);
		in_vec[i + book->order] = in_buf[i] - prediction[i];
		e[i] = (f32) in_vec[i + book->order];
	}

	for (i = 0; i < book->order; i++)
		in_vec[i] = prediction[8 - book->order + i] + in_vec[8 + i];

	for (i = 0; i < 8; i++) {
		prediction[8 + i] = inner_product(book->order + i, book->coef_tbl[optimalp][i], in_vec);
		in_vec[i + book->order] = in_buf[8 + i] - prediction[8 + i];
		e[8 + i] = (f32) in_vec[i + book->order];
	}

	int32_t ie[16];
	int32_t max = 0;

    // Clamp the errors to 16-bit signed ints, and put them in ie.
	clampv(16, e, ie, 16);

    // Find a value with highest absolute value.
    // Reproduce original tool bug:
    //      If this first finds -2^n and later 2^n, it should set 'max' to the
    //      latter, which needs a higher value for 'scale'.
	for (i = 0; i < 16; i++)
		if (math_absF(ie[i]) > math_absF(max))
			max = ie[i];

    // Compute which power of two we need to scale down by in order to make
    // all values representable as 4-bit (resp. 2-bit) signed integers
    // (i.e. be in [-8, 7] (resp. [-2, 1])).
    // The worst-case 'max' is -2^15, so this will be at most 12 (resp. 14).
    // Depends on whether we are encoding for long or short VADPCM.

	int32_t enc_bits = (frame_size == 5) ? 2 : 4;
	int32_t scale_fac = 16 - enc_bits;
	int32_t llevel = -(1 << (enc_bits - 1));
	int32_t ulevel = -llevel - 1;
	int32_t scale;

	for (scale = 0; scale <= scale_fac; scale++) {
		if (max <= ulevel && max >= llevel) {
			break;
		}
		max /= 2;
	}

	int32_t saveState[16];

	for (i = 0; i < 16; i++)
		saveState[i] = ctx->state[i];

	int16_t ix[16];
	int32_t nIter;
	int32_t cV;
	int32_t maxClip;

    // Try with the computed scale, but if it turns out we don't fit in 4 bits
    // (if some |cV| >= 2), use scale + 1 instead (i.e. downscaling by another
    // factor of 2).
	scale--;
	nIter = 0;
	do {
		nIter++;
		maxClip = 0;
		scale++;

		if (scale > scale_fac)
			scale = scale_fac;

        // Copy over the last 'order' samples from the previous output.
		for (i = 0; i < book->order; i++)
			in_vec[i] = saveState[16 - book->order + i];

        // For 8 samples...
		for (i = 0; i < 8; i++) {
            // Compute a prediction based on 'order' values from the old state,
            // plus previous *quantized* errors in this chunk (because that's
            // all the decoder will have available).
			prediction[i] = inner_product(book->order + i, book->coef_tbl[optimalp][i], in_vec);

            // Compute the error, and divide it by 2^scale, rounding to the
            // nearest integer. This should ideally result in a 4-bit integer.
			se = (f32) in_buf[i] - (f32) prediction[i];
			ix[i] = qsample(se, 1 << scale);

            // Clamp the error to a 4-bit signed integer, and record what delta
            // was needed for that.
			cV = (int16_t) clip(ix[i], llevel, ulevel) - ix[i];
			if (maxClip < abs(cV))
				maxClip = abs(cV);
			ix[i] += cV;

            // Record the quantized error in inVector for later predictions,
            // and the quantized (decoded) output in state (for use in the next
            // batch of 8 samples).
			in_vec[i + book->order] = ix[i] * (1 << scale);
			ctx->state[i] = prediction[i] + in_vec[i + book->order];
		}

        // Copy over the last 'order' decoded samples from the above chunk.
		for (i = 0; i < book->order; i++)
			in_vec[i] = ctx->state[8 - book->order + i];

		// ... and do the same thing as before.
		for (i = 0; i < 8; i++) {
			prediction[8 + i] = inner_product(book->order + i, book->coef_tbl[optimalp][i], in_vec);

			se = (f32) in_buf[8 + i] - (f32) prediction[8 + i];
			ix[8 + i] = qsample(se, 1 << scale);

			cV = (int16_t) clip(ix[8 + i], llevel, ulevel) - ix[8 + i];
			if (maxClip < abs(cV)) {
				maxClip = abs(cV);
			}
			ix[8 + i] += cV;

			in_vec[i + book->order] = ix[8 + i] * (1 << scale);
			ctx->state[8 + i] = prediction[8 + i] + in_vec[i + book->order];
		}
	} while (maxClip >= 2 && nIter < 2);

    // The scale, the predictor index, and the 16 computed outputs are now all
    // 4-bit numbers. Write them out as either 1 + 8 bytes or 1 + 4 bytes depending
    // on whether we are encoding for long or short VADPCM.

	uint8_t* p = ctx->output;
	*(p++) = (scale << 4) | (optimalp & 0xf);

	switch (frame_size) {
		case 5:
			for (i = 0; i < 16; i += 4)
				*(p++) = ((ix[i] & 0x3) << 6) | ((ix[i + 1] & 0x3) << 4) | ((ix[i + 2] & 0x3) << 2) | (ix[i + 3] & 0x3);
		case 9:
			for (i = 0; i < 16; i += 2) {
				*(p++) = (ix[i] << 4) | (ix[i + 1] & 0xf);
			}
			break;
	}

}

///////////////////////////////////////////////////////////////////////////////
