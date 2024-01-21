#include "cclib.h"
#include <stdint.h>

typedef char* (*sfunc)(const char*, const char*);
typedef char* (*nsfunc)(const char*, size_t, const char*);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

static inline int __impl_strnrep(char* str, size_t len, sfunc srch, nsfunc nsrch, const char* mtch, const char* rep, int num, int start) {
	char* o = str;
	int mtch_len = strlen(mtch);
	int rep_len = strlen(rep);
	int max_len;
	int nlen;

	if (!str || !*str)
		return 0;

	switch (len) {
	case 0:
		max_len = 0;
		nlen = len = strlen(str);
		break;

	default:
		max_len = len;
		nlen = len = strnlen(str, max_len);
		break;
	}

	int replace = !!srch;

	while (
		(srch && (str = srch(str, mtch)) ) ||
		(nsrch && (str = nsrch(str, max_len ? 0 : len - (str - o), mtch)) )
	) {
		if (start > 0) {
			start--;
			str++;
			continue;
		}

		int point = (int)((uintptr_t)str - (uintptr_t)o);
		int mve_len = nlen - (point + mtch_len);
		int oins_len = rep_len;
		int ins_len = rep_len;

		if (max_len) {
			oins_len = ins_len;

			mve_len = clamp(mve_len + point, 0, max_len) - point;
			ins_len = clamp(ins_len + point, 0, max_len) - point;
		}

		if (replace) {
			if (oins_len == ins_len && mve_len > 0)
				memmove(str + rep_len, str + mtch_len, mve_len );

			if (ins_len > 0)
				memcpy(str, rep, ins_len);
		}

		nlen += (ins_len - mtch_len);
		if (max_len) nlen = clamp_max(nlen, max_len);

		if (replace)
			o[nlen] = '\0';

		if (max_len) {
			if (oins_len != ins_len)
				break;
		}

		if (replace)
			str += ins_len;
		else
			str += mtch_len;

		if (!decr(num))
			break;
	}

	return nlen - len;
}

static inline char* strnvalid(const char* str, size_t n) {
	const char* t = str;
	size_t consumed = 0;

	while ((void)(consumed = (size_t)(str - t)), consumed < n && '\0' != *str) {
		const size_t remaining = n - consumed;

		if (0xf0 == (0xf8 & *str)) {

			if (remaining < 4) {
				return (char*)str;
			}

			if ((0x80 != (0xc0 & str[1])) || (0x80 != (0xc0 & str[2])) ||
				(0x80 != (0xc0 & str[3]))) {
				return (char*)str;
			}

			if ((remaining != 4) && (0x80 == (0xc0 & str[4]))) {
				return (char*)str;
			}

			if ((0 == (0x07 & str[0])) && (0 == (0x30 & str[1]))) {
				return (char*)str;
			}

			str += 4;
		} else if (0xe0 == (0xf0 & *str)) {

			if (remaining < 3) {
				return (char*)str;
			}

			if ((0x80 != (0xc0 & str[1])) || (0x80 != (0xc0 & str[2]))) {
				return (char*)str;
			}

			if ((remaining != 3) && (0x80 == (0xc0 & str[3]))) {
				return (char*)str;
			}

			if ((0 == (0x0f & str[0])) && (0 == (0x20 & str[1]))) {
				return (char*)str;
			}

			str += 3;
		} else if (0xc0 == (0xe0 & *str)) {

			if (remaining < 2) {
				return (char*)str;
			}

			if (0x80 != (0xc0 & str[1])) {
				return (char*)str;
			}

			if ((remaining != 2) && (0x80 == (0xc0 & str[2]))) {
				return (char*)str;
			}

			if (0 == (0x1e & str[0])) {
				return (char*)str;
			}

			str += 2;
		} else if (0x00 == (0x80 & *str)) {

			str += 1;
		} else {

			return (char*)str;
		}
	}

	return NULL;
}

static inline char* strvalid(const char* str) {
	return strnvalid(str, SIZE_MAX);
}

static inline char* strcatcodepoint(char* str, int chr, size_t n) {
	if (0 == ((int)0xffffff80 & chr)) {

		if (n < 1) {
			return NULL;
		}
		str[0] = (char)chr;
		str += 1;
	} else if (0 == ((int)0xfffff800 & chr)) {

		if (n < 2) {
			return NULL;
		}
		str[0] = (char)(0xc0 | (char)((chr >> 6) & 0x1f));
		str[1] = (char)(0x80 | (char)(chr & 0x3f));
		str += 2;
	} else if (0 == ((int)0xffff0000 & chr)) {

		if (n < 3) {
			return NULL;
		}
		str[0] = (char)(0xe0 | (char)((chr >> 12) & 0x0f));
		str[1] = (char)(0x80 | (char)((chr >> 6) & 0x3f));
		str[2] = (char)(0x80 | (char)(chr & 0x3f));
		str += 3;
	} else {

		if (n < 4) {
			return NULL;
		}
		str[0] = (char)(0xf0 | (char)((chr >> 18) & 0x07));
		str[1] = (char)(0x80 | (char)((chr >> 12) & 0x3f));
		str[2] = (char)(0x80 | (char)((chr >> 6) & 0x3f));
		str[3] = (char)(0x80 | (char)(chr & 0x3f));
		str += 4;
	}

	return str;
}

static inline char* strcodepoint(const char* str, int* out_codepoint) {
	if (0xf0 == (0xf8 & str[0])) {

		*out_codepoint = ((0x07 & str[0]) << 18) | ((0x3f & str[1]) << 12) |
			((0x3f & str[2]) << 6) | (0x3f & str[3]);
		str += 4;
	} else if (0xe0 == (0xf0 & str[0])) {

		*out_codepoint =
			((0x0f & str[0]) << 12) | ((0x3f & str[1]) << 6) | (0x3f & str[2]);
		str += 3;
	} else if (0xc0 == (0xe0 & str[0])) {

		*out_codepoint = ((0x1f & str[0]) << 6) | (0x3f & str[1]);
		str += 2;
	} else {

		*out_codepoint = str[0];
		str += 1;
	}

	return (char*)str;
}

static inline int strmakevalid(char* str, const int replacement) {
	char* read = str;
	char* write = read;
	const char r = (char)replacement;
	int codepoint = 0;

	if (replacement > 0x7f) {
		return -1;
	}

	while ('\0' != *read) {
		if (0xf0 == (0xf8 & *read)) {

			if ((0x80 != (0xc0 & read[1])) || (0x80 != (0xc0 & read[2])) ||
				(0x80 != (0xc0 & read[3]))) {
				*write++ = r;
				read++;
				continue;
			}

			read = strcodepoint(read, &codepoint);
			write = strcatcodepoint(write, codepoint, 4);
		} else if (0xe0 == (0xf0 & *read)) {

			if ((0x80 != (0xc0 & read[1])) || (0x80 != (0xc0 & read[2]))) {
				*write++ = r;
				read++;
				continue;
			}

			read = strcodepoint(read, &codepoint);
			write = strcatcodepoint(write, codepoint, 3);
		} else if (0xc0 == (0xe0 & *read)) {

			if (0x80 != (0xc0 & read[1])) {
				*write++ = r;
				read++;
				continue;
			}

			read = strcodepoint(read, &codepoint);
			write = strcatcodepoint(write, codepoint, 2);
		} else if (0x00 == (0x80 & *read)) {

			read = strcodepoint(read, &codepoint);
			write = strcatcodepoint(write, codepoint, 1);
		} else {

			*write++ = r;
			read++;
			continue;
		}
	}

	*write = '\0';

	return 0;
}

static inline size_t strcodepointcalcsize(const char* str) {
	if (0xf0 == (0xf8 & str[0])) {

		return 4;
	} else if (0xe0 == (0xf0 & str[0])) {

		return 3;
	} else if (0xc0 == (0xe0 & str[0])) {

		return 2;
	}

	return 1;
}

static inline size_t strcodepointsize(int chr) {
	if (0 == ((int)0xffffff80 & chr)) {
		return 1;
	} else if (0 == ((int)0xfffff800 & chr)) {
		return 2;
	} else if (0 == ((int)0xffff0000 & chr)) {
		return 3;
	} else {
		return 4;
	}
}

static inline int strlwrcodepoint(int cp) {
	if (((0x0041 <= cp) && (0x005a >= cp)) ||
		((0x00c0 <= cp) && (0x00d6 >= cp)) ||
		((0x00d8 <= cp) && (0x00de >= cp)) ||
		((0x0391 <= cp) && (0x03a1 >= cp)) ||
		((0x03a3 <= cp) && (0x03ab >= cp)) ||
		((0x0410 <= cp) && (0x042f >= cp))) {
		cp += 32;
	} else if ((0x0400 <= cp) && (0x040f >= cp)) {
		cp += 80;
	} else if (((0x0100 <= cp) && (0x012f >= cp)) ||
		((0x0132 <= cp) && (0x0137 >= cp)) ||
		((0x014a <= cp) && (0x0177 >= cp)) ||
		((0x0182 <= cp) && (0x0185 >= cp)) ||
		((0x01a0 <= cp) && (0x01a5 >= cp)) ||
		((0x01de <= cp) && (0x01ef >= cp)) ||
		((0x01f8 <= cp) && (0x021f >= cp)) ||
		((0x0222 <= cp) && (0x0233 >= cp)) ||
		((0x0246 <= cp) && (0x024f >= cp)) ||
		((0x03d8 <= cp) && (0x03ef >= cp)) ||
		((0x0460 <= cp) && (0x0481 >= cp)) ||
		((0x048a <= cp) && (0x04ff >= cp))) {
		cp |= 0x1;
	} else if (((0x0139 <= cp) && (0x0148 >= cp)) ||
		((0x0179 <= cp) && (0x017e >= cp)) ||
		((0x01af <= cp) && (0x01b0 >= cp)) ||
		((0x01b3 <= cp) && (0x01b6 >= cp)) ||
		((0x01cd <= cp) && (0x01dc >= cp))) {
		cp += 1;
		cp &= ~0x1;
	} else {
		switch (cp) {
		default:
			break;
		case 0x0178:
			cp = 0x00ff;
			break;
		case 0x0243:
			cp = 0x0180;
			break;
		case 0x018e:
			cp = 0x01dd;
			break;
		case 0x023d:
			cp = 0x019a;
			break;
		case 0x0220:
			cp = 0x019e;
			break;
		case 0x01b7:
			cp = 0x0292;
			break;
		case 0x01c4:
			cp = 0x01c6;
			break;
		case 0x01c7:
			cp = 0x01c9;
			break;
		case 0x01ca:
			cp = 0x01cc;
			break;
		case 0x01f1:
			cp = 0x01f3;
			break;
		case 0x01f7:
			cp = 0x01bf;
			break;
		case 0x0187:
			cp = 0x0188;
			break;
		case 0x018b:
			cp = 0x018c;
			break;
		case 0x0191:
			cp = 0x0192;
			break;
		case 0x0198:
			cp = 0x0199;
			break;
		case 0x01a7:
			cp = 0x01a8;
			break;
		case 0x01ac:
			cp = 0x01ad;
			break;
		case 0x01af:
			cp = 0x01b0;
			break;
		case 0x01b8:
			cp = 0x01b9;
			break;
		case 0x01bc:
			cp = 0x01bd;
			break;
		case 0x01f4:
			cp = 0x01f5;
			break;
		case 0x023b:
			cp = 0x023c;
			break;
		case 0x0241:
			cp = 0x0242;
			break;
		case 0x03fd:
			cp = 0x037b;
			break;
		case 0x03fe:
			cp = 0x037c;
			break;
		case 0x03ff:
			cp = 0x037d;
			break;
		case 0x037f:
			cp = 0x03f3;
			break;
		case 0x0386:
			cp = 0x03ac;
			break;
		case 0x0388:
			cp = 0x03ad;
			break;
		case 0x0389:
			cp = 0x03ae;
			break;
		case 0x038a:
			cp = 0x03af;
			break;
		case 0x038c:
			cp = 0x03cc;
			break;
		case 0x038e:
			cp = 0x03cd;
			break;
		case 0x038f:
			cp = 0x03ce;
			break;
		case 0x0370:
			cp = 0x0371;
			break;
		case 0x0372:
			cp = 0x0373;
			break;
		case 0x0376:
			cp = 0x0377;
			break;
		case 0x03f4:
			cp = 0x03b8;
			break;
		case 0x03cf:
			cp = 0x03d7;
			break;
		case 0x03f9:
			cp = 0x03f2;
			break;
		case 0x03f7:
			cp = 0x03f8;
			break;
		case 0x03fa:
			cp = 0x03fb;
			break;
		}
	}

	return cp;
}

static inline int struprcodepoint(int cp) {
	if (((0x0061 <= cp) && (0x007a >= cp)) ||
		((0x00e0 <= cp) && (0x00f6 >= cp)) ||
		((0x00f8 <= cp) && (0x00fe >= cp)) ||
		((0x03b1 <= cp) && (0x03c1 >= cp)) ||
		((0x03c3 <= cp) && (0x03cb >= cp)) ||
		((0x0430 <= cp) && (0x044f >= cp))) {
		cp -= 32;
	} else if ((0x0450 <= cp) && (0x045f >= cp)) {
		cp -= 80;
	} else if (((0x0100 <= cp) && (0x012f >= cp)) ||
		((0x0132 <= cp) && (0x0137 >= cp)) ||
		((0x014a <= cp) && (0x0177 >= cp)) ||
		((0x0182 <= cp) && (0x0185 >= cp)) ||
		((0x01a0 <= cp) && (0x01a5 >= cp)) ||
		((0x01de <= cp) && (0x01ef >= cp)) ||
		((0x01f8 <= cp) && (0x021f >= cp)) ||
		((0x0222 <= cp) && (0x0233 >= cp)) ||
		((0x0246 <= cp) && (0x024f >= cp)) ||
		((0x03d8 <= cp) && (0x03ef >= cp)) ||
		((0x0460 <= cp) && (0x0481 >= cp)) ||
		((0x048a <= cp) && (0x04ff >= cp))) {
		cp &= ~0x1;
	} else if (((0x0139 <= cp) && (0x0148 >= cp)) ||
		((0x0179 <= cp) && (0x017e >= cp)) ||
		((0x01af <= cp) && (0x01b0 >= cp)) ||
		((0x01b3 <= cp) && (0x01b6 >= cp)) ||
		((0x01cd <= cp) && (0x01dc >= cp))) {
		cp -= 1;
		cp |= 0x1;
	} else {
		switch (cp) {
		default:
			break;
		case 0x00ff:
			cp = 0x0178;
			break;
		case 0x0180:
			cp = 0x0243;
			break;
		case 0x01dd:
			cp = 0x018e;
			break;
		case 0x019a:
			cp = 0x023d;
			break;
		case 0x019e:
			cp = 0x0220;
			break;
		case 0x0292:
			cp = 0x01b7;
			break;
		case 0x01c6:
			cp = 0x01c4;
			break;
		case 0x01c9:
			cp = 0x01c7;
			break;
		case 0x01cc:
			cp = 0x01ca;
			break;
		case 0x01f3:
			cp = 0x01f1;
			break;
		case 0x01bf:
			cp = 0x01f7;
			break;
		case 0x0188:
			cp = 0x0187;
			break;
		case 0x018c:
			cp = 0x018b;
			break;
		case 0x0192:
			cp = 0x0191;
			break;
		case 0x0199:
			cp = 0x0198;
			break;
		case 0x01a8:
			cp = 0x01a7;
			break;
		case 0x01ad:
			cp = 0x01ac;
			break;
		case 0x01b0:
			cp = 0x01af;
			break;
		case 0x01b9:
			cp = 0x01b8;
			break;
		case 0x01bd:
			cp = 0x01bc;
			break;
		case 0x01f5:
			cp = 0x01f4;
			break;
		case 0x023c:
			cp = 0x023b;
			break;
		case 0x0242:
			cp = 0x0241;
			break;
		case 0x037b:
			cp = 0x03fd;
			break;
		case 0x037c:
			cp = 0x03fe;
			break;
		case 0x037d:
			cp = 0x03ff;
			break;
		case 0x03f3:
			cp = 0x037f;
			break;
		case 0x03ac:
			cp = 0x0386;
			break;
		case 0x03ad:
			cp = 0x0388;
			break;
		case 0x03ae:
			cp = 0x0389;
			break;
		case 0x03af:
			cp = 0x038a;
			break;
		case 0x03cc:
			cp = 0x038c;
			break;
		case 0x03cd:
			cp = 0x038e;
			break;
		case 0x03ce:
			cp = 0x038f;
			break;
		case 0x0371:
			cp = 0x0370;
			break;
		case 0x0373:
			cp = 0x0372;
			break;
		case 0x0377:
			cp = 0x0376;
			break;
		case 0x03d1:
			cp = 0x0398;
			break;
		case 0x03d7:
			cp = 0x03cf;
			break;
		case 0x03f2:
			cp = 0x03f9;
			break;
		case 0x03f8:
			cp = 0x03f7;
			break;
		case 0x03fb:
			cp = 0x03fa;
			break;
		}
	}

	return cp;
}

static inline char* strrcodepoint(const char* str,
	int* out_codepoint) {
	const char* s = (const char*)str;

	if (0xf0 == (0xf8 & s[0])) {

		*out_codepoint = ((0x07 & s[0]) << 18) | ((0x3f & s[1]) << 12) |
			((0x3f & s[2]) << 6) | (0x3f & s[3]);
	} else if (0xe0 == (0xf0 & s[0])) {

		*out_codepoint =
			((0x0f & s[0]) << 12) | ((0x3f & s[1]) << 6) | (0x3f & s[2]);
	} else if (0xc0 == (0xe0 & s[0])) {

		*out_codepoint = ((0x1f & s[0]) << 6) | (0x3f & s[1]);
	} else {

		*out_codepoint = s[0];
	}

	do {
		s--;
	} while ((0 != (0x80 & s[0])) && (0x80 == (0xc0 & s[0])));

	return (char*)s;
}

#pragma GCC diagnostic pop

// Lib Override ///////////////////////////////////////////////////////////////

/*
   size_t strlen(const char* s) {
    const char* src = s;

    if (s + 1 == (void*)1)
        return 0;

    // check while non-aligned
    for (; ((uintptr_t)s) & 0x3; s++)
        if (!*s) return s - src;

    uint32_t* u = (void*)s;

    for (;; u++) {
        if (hasnullbyte32(*u)) {
            char* c = (char*)u;

            if (!c[0]) return (c - src);
            if (!c[1]) return (c - src) + 1;
            if (!c[2]) return (c - src) + 2;
            if (!c[3]) return (c - src) + 3;
        }
    }
   }

   char* strchr(const char* src, int chr) {
    char c[5] = { '\0', '\0', '\0', '\0', '\0' };

    if (0 == chr) {

        while ('\0' != *src) {
            src++;
        }
        return (char*)src;
    } else if (0 == ((int)0xffffff80 & chr)) {

        c[0] = (char)chr;
    } else if (0 == ((int)0xfffff800 & chr)) {

        c[0] = (char)(0xc0 | (char)(chr >> 6));
        c[1] = (char)(0x80 | (char)(chr & 0x3f));
    } else if (0 == ((int)0xffff0000 & chr)) {

        c[0] = (char)(0xe0 | (char)(chr >> 12));
        c[1] = (char)(0x80 | (char)((chr >> 6) & 0x3f));
        c[2] = (char)(0x80 | (char)(chr & 0x3f));
    } else {

        c[0] = (char)(0xf0 | (char)(chr >> 18));
        c[1] = (char)(0x80 | (char)((chr >> 12) & 0x3f));
        c[2] = (char)(0x80 | (char)((chr >> 6) & 0x3f));
        c[3] = (char)(0x80 | (char)(chr & 0x3f));
    }

    return strstr(src, c);
   }

   int strcmp(const char* src1, const char* src2) {
    while (('\0' != *src1) || ('\0' != *src2)) {
        if (*src1 < *src2) {
            return -1;
        } else if (*src1 > *src2) {
            return 1;
        }

        src1++;
        src2++;
    }

    return 0;
   }

   int strncasecmp(const char* src1, const char* src2, size_t n) {
    int src1_lwr_cp = 0, src2_lwr_cp = 0, src1_upr_cp = 0,
        src2_upr_cp = 0, src1_orig_cp = 0, src2_orig_cp = 0;

    do {
        const char* const s1 = src1;
        const char* const s2 = src2;

        if (0 == n)
            return 0;

        if ((1 == n) && ((0xc0 == (0xe0 & *s1)) || (0xc0 == (0xe0 & *s2)))) {
            const int c1 = (0xe0 & *s1);
            const int c2 = (0xe0 & *s2);

            if (c1 < c2)
                return c1 - c2;
            else
                return 0;
        }

        if ((2 >= n) && ((0xe0 == (0xf0 & *s1)) || (0xe0 == (0xf0 & *s2)))) {
            const int c1 = (0xf0 & *s1);
            const int c2 = (0xf0 & *s2);

            if (c1 < c2)
                return c1 - c2;
            else
                return 0;
        }

        if ((3 >= n) && ((0xf0 == (0xf8 & *s1)) || (0xf0 == (0xf8 & *s2)))) {
            const int c1 = (0xf8 & *s1);
            const int c2 = (0xf8 & *s2);

            if (c1 < c2)
                return c1 - c2;
            else
                return 0;
        }

        src1 = strcodepoint(src1, &src1_orig_cp);
        src2 = strcodepoint(src2, &src2_orig_cp);
        n -= strcodepointsize(src1_orig_cp);

        src1_lwr_cp = strlwrcodepoint(src1_orig_cp);
        src2_lwr_cp = strlwrcodepoint(src2_orig_cp);

        src1_upr_cp = struprcodepoint(src1_orig_cp);
        src2_upr_cp = struprcodepoint(src2_orig_cp);

        if ((0 == src1_orig_cp) && (0 == src2_orig_cp))
            return 0;
        else if ((src1_lwr_cp == src2_lwr_cp) || (src1_upr_cp == src2_upr_cp))
            continue;

        return src1_lwr_cp - src2_lwr_cp;
    } while (0 < n);

    return 0;
   }

   int strcasecmp(const char* src1, const char* src2) {
    int src1_lwr_cp = 0, src2_lwr_cp = 0, src1_upr_cp = 0,
        src2_upr_cp = 0, src1_orig_cp = 0, src2_orig_cp = 0;

    for (;;) {
        src1 = strcodepoint(src1, &src1_orig_cp);
        src2 = strcodepoint(src2, &src2_orig_cp);

        src1_lwr_cp = strlwrcodepoint(src1_orig_cp);
        src2_lwr_cp = strlwrcodepoint(src2_orig_cp);

        src1_upr_cp = struprcodepoint(src1_orig_cp);
        src2_upr_cp = struprcodepoint(src2_orig_cp);

        if ((0 == src1_orig_cp) && (0 == src2_orig_cp))
            return 0;

        else if ((src1_lwr_cp == src2_lwr_cp) || (src1_upr_cp == src2_upr_cp))
            continue;

        return src1_lwr_cp - src2_lwr_cp;
    }
   }

   char* strrchr(const char* src, int chr) {

    char* match = NULL;
    char c[5] = { '\0', '\0', '\0', '\0', '\0' };

    if (0 == chr) {

        while ('\0' != *src) {
            src++;
        }
        return (char*)src;
    } else if (0 == ((int)0xffffff80 & chr)) {

        c[0] = (char)chr;
    } else if (0 == ((int)0xfffff800 & chr)) {

        c[0] = (char)(0xc0 | (char)(chr >> 6));
        c[1] = (char)(0x80 | (char)(chr & 0x3f));
    } else if (0 == ((int)0xffff0000 & chr)) {

        c[0] = (char)(0xe0 | (char)(chr >> 12));
        c[1] = (char)(0x80 | (char)((chr >> 6) & 0x3f));
        c[2] = (char)(0x80 | (char)(chr & 0x3f));
    } else {

        c[0] = (char)(0xf0 | (char)(chr >> 18));
        c[1] = (char)(0x80 | (char)((chr >> 12) & 0x3f));
        c[2] = (char)(0x80 | (char)((chr >> 6) & 0x3f));
        c[3] = (char)(0x80 | (char)(chr & 0x3f));
    }

    while ('\0' != *src) {
        size_t offset = 0;

        while (src[offset] == c[offset]) {
            offset++;
        }

        if ('\0' == c[offset]) {

            match = (char*)src;
            src += offset;
        } else {
            src += offset;

            if ('\0' != *src) {
                do {
                    src++;
                } while (0x80 == (0xc0 & *src));
            }
        }
    }

    return match;
   }

   char* strpbrk(const char* str, const char* accept) {
    while ('\0' != *str) {
        const char* a = accept;
        size_t offset = 0;

        while ('\0' != *a) {

            if ((0x80 != (0xc0 & *a)) && (0 < offset)) {
                return (char*)str;
            } else {
                if (*a == str[offset]) {

                    offset++;
                    a++;
                } else {

                    do {
                        a++;
                    } while (0x80 == (0xc0 & *a));

                    offset = 0;
                }
            }
        }

        if (0 < offset) {
            return (char*)str;
        }

        do {
            str++;
        } while ((0x80 == (0xc0 & *str)));
    }

    return NULL;
   }

   size_t strspn(const char* src, const char* accept) {
    size_t chars = 0;

    while ('\0' != *src) {
        const char* a = accept;
        size_t offset = 0;

        while ('\0' != *a) {

            if ((0x80 != (0xc0 & *a)) && (0 < offset)) {

                chars++;
                src += offset;
                offset = 0;
                break;
            } else {
                if (*a == src[offset]) {
                    offset++;
                    a++;
                } else {

                    do {
                        a++;
                    } while (0x80 == (0xc0 & *a));

                    offset = 0;
                }
            }
        }

        if (0 < offset) {
            chars++;
            src += offset;
            continue;
        }

        if ('\0' == *a) {
            return chars;
        }
    }

    return chars;
   }

   size_t strcspn(const char* src, const char* reject) {
    size_t chars = 0;

    while ('\0' != *src) {
        const char* r = reject;
        size_t offset = 0;

        while ('\0' != *r) {

            if ((0x80 != (0xc0 & *r)) && (0 < offset)) {
                return chars;
            } else {
                if (*r == src[offset]) {

                    offset++;
                    r++;
                } else {

                    do {
                        r++;
                    } while (0x80 == (0xc0 & *r));

                    offset = 0;
                }
            }
        }

        if (0 < offset) {
            return chars;
        }

        do {
            src++;
        } while ((0x80 == (0xc0 & *src)));
        chars++;
    }

    return chars;
   }
 */

char* strstr(const char* hay, const char* needle) {
	return memmem(hay, strlen(hay), needle, strlen(needle));
}

char* strnstr(const char* hay, size_t n, const char* needle) {
	return memmem(hay, strnlen(hay, n), needle, strlen(needle));
}

char* strcasestr(const char* haystack, const char* needle) {

	if ('\0' == *needle) {
		return (char*)haystack;
	}

	for (;;) {
		const char* maybeMatch = haystack;
		const char* n = needle;
		int h_cp = 0, n_cp = 0;

		const char* nextH = haystack = strcodepoint(haystack, &h_cp);
		n = strcodepoint(n, &n_cp);

		while ((0 != h_cp) && (0 != n_cp)) {
			h_cp = strlwrcodepoint(h_cp);
			n_cp = strlwrcodepoint(n_cp);

			if (h_cp != n_cp) {
				break;
			}

			haystack = strcodepoint(haystack, &h_cp);
			n = strcodepoint(n, &n_cp);
		}

		if (0 == n_cp) {

			return (char*)maybeMatch;
		}

		if (0 == h_cp) {

			return NULL;
		}

		haystack = nextH;
	}
}

char* strlwr(char* str) {
	char* og = str;
	int cp = 0;
	char* pn = strcodepoint(str, &cp);

	while (cp != 0) {
		const int lwr_cp = strlwrcodepoint(cp);
		const size_t size = strcodepointsize(lwr_cp);

		if (lwr_cp != cp) {
			strcatcodepoint(str, lwr_cp, size);
		}

		str = pn;
		pn = strcodepoint(str, &cp);
	}

	return og;
}

char* strupr(char* str) {
	char* og = str;
	int cp = 0;
	char* pn = strcodepoint(str, &cp);

	while (cp != 0) {
		const int lwr_cp = struprcodepoint(cp);
		const size_t size = strcodepointsize(lwr_cp);

		if (lwr_cp != cp) {
			strcatcodepoint(str, lwr_cp, size);
		}

		str = pn;
		pn = strcodepoint(str, &cp);
	}

	return og;
}

char* strflipsep(char* s) {
	if (strchr(s, '\\'))
		strrep(s, "\\", "/");

	return s;
}

///////////////////////////////////////////////////////////////////////////////

size_t strcnlen(const char* str, size_t n) {
	const char* t = str;
	size_t length = 0;

	while ((size_t)(str - t) < n && '\0' != *str) {
		if (0xf0 == (0xf8 & *str)) {

			str += 4;
		} else if (0xe0 == (0xf0 & *str)) {

			str += 3;
		} else if (0xc0 == (0xe0 & *str)) {

			str += 2;
		} else {

			str += 1;
		}

		length++;
	}

	if ((size_t)(str - t) > n) {
		length--;
	}
	return length;
}

size_t strclen(const char* str) {
	return strnlen(str, __INT_MAX__);
}

int strrep(char* src, const char* mtch, const char* rep) {
	return __impl_strnrep(src, 0, strstr, NULL, mtch, rep, -1, -1);
}

int strcrep(const char* str, const char* mtch, const char* rep) {
	return __impl_strnrep((char*)str, 0, NULL, strnstr, mtch, rep, -1, -1);
}

int strnrep(char* src, int len, const char* mtch, const char* rep) {
	return __impl_strnrep(src, len - 1, strstr, NULL, mtch, rep, -1, -1);
}

int strislower(int chr) {
	return chr != struprcodepoint(chr);
}

int strisupper(int chr) {
	return chr != strlwrcodepoint(chr);
}

int strneq(const char* a, const char* b, size_t n) {
	for (; *a && *b && n; a++, b++, n--)
		if (*a != *b)
			return 0;
	return 1;
}

int streq(const char* a, const char* b) {
	for (;; a++, b++) {
		if (*a != *b) return false;
		if (!*a) return true;
	}
}

char* __m_strend(const char* src, const char* end, int elen) {
	int slen = strlen(src);

	if (slen < elen)
		return NULL;
	src += slen - elen;

	if (memeq(src, end, elen))
		return (void*)src;
	return NULL;
}

char* __m_strnend(const char* src, const char* end, int elen, int n) {
	int slen = strnlen(src, n);

	if (slen < elen)
		return NULL;
	src += slen - elen;

	if (memeq(src, end, elen))
		return (void*)src;
	return NULL;
}

char* __m_strfend(const char* src, const char* end, int elen, int slen) {
	if (slen < elen)
		return NULL;
	src += slen - elen;

	if (memeq(src, end, elen))
		return (void*)src;
	return NULL;
}

char* __m_strstart(const char* src, const char* begin, int blen) {
	int slen = strnlen(src, blen);

	if (slen < blen) return NULL;
	if (memeq(src, begin, blen)) return (void*)src;
	return NULL;
}

bool chrspn(char __c, const char* __spn) {
	uint8_t c = __c;
	uint8_t* s = (uint8_t*)__spn;
	char tbl[256] = {};

	for (; *s; s++)
		tbl[*s] = true;

	return tbl[c];
}

///////////////////////////////////////////////////////////////////////////////

static inline int hexspn(const char* s) {
	const char valid_hex[] = {
		['0'] = 1, ['1'] = 1, ['2'] = 1, ['3'] = 1,
		['4'] = 1, ['5'] = 1, ['6'] = 1, ['7'] = 1,
		['8'] = 1, ['9'] = 1, ['A'] = 1, ['B'] = 1,
		['C'] = 1, ['D'] = 1, ['E'] = 1, ['F'] = 1,

		['a'] = 1, ['b'] = 1, ['c'] = 1, ['d'] = 1,
		['e'] = 1, ['f'] = 1,

		['x'] = 1,
	};
	const char* p = s;

	for (; valid_hex[(uint8_t)*p]; p++);

	return p - s;
#undef VALID
}

uint32_t strto_u32(const char* s) {
	const char val_map[] = {
		['0'] = 0x0, ['1'] = 0x1, ['2'] = 0x2, ['3'] = 0x3,
		['4'] = 0x4, ['5'] = 0x5, ['6'] = 0x6, ['7'] = 0x7,
		['8'] = 0x8, ['9'] = 0x9, ['A'] = 0xA, ['B'] = 0xB,
		['C'] = 0xC, ['D'] = 0xD, ['E'] = 0xE, ['F'] = 0xF,

		['a'] = 0xA, ['b'] = 0xB, ['c'] = 0xC, ['d'] = 0xD,
		['e'] = 0xE, ['f'] = 0xF,
	};
	const char* v = s + hexspn(s) - 1;

	uint32_t shift = 0;
	uint32_t o = 0;

	for (; shift < 32 && v > s; shift += 4, v--)
		o |= val_map[(uint8_t)*v] << shift;

	return o;
}

char* fmtw_u32(char* p, uint32_t val) {
	uint8_t valmap[] = {
		'0', '1', '2', '3',
		'4', '5', '6', '7',
		'8', '9', 'A', 'B',
		'C', 'D', 'E', 'F',
	};

	for (int i = 28; i >= 0; i -= 4)
		*(p++) = valmap[(val >> i) & 0xF];

	return p;
}
