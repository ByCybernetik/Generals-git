/*
** UTF-8 <-> UTF-16 (Windows WCHAR / -fshort-wchar) conversion helpers.
** Used by Linux Win32 MultiByteToWideChar / WideCharToMultiByte and string
** translate paths. Operates on 16-bit code units so it stays ABI-compatible
** with Generals UnicodeString regardless of host wchar_t size.
*/
#ifndef GENERALS_UTF8_CONVERT_H
#define GENERALS_UTF8_CONVERT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Decode one UTF-8 sequence. Returns bytes consumed (>0), or -1 on error. */
static inline int generals_utf8_decode(
	const unsigned char *src,
	size_t src_len,
	uint32_t *out_cp)
{
	unsigned char c0;
	if (src == NULL || src_len == 0 || out_cp == NULL) {
		return -1;
	}
	c0 = src[0];
	if (c0 < 0x80u) {
		*out_cp = c0;
		return 1;
	}
	if ((c0 & 0xE0u) == 0xC0u) {
		unsigned char c1;
		uint32_t cp;
		if (src_len < 2) {
			return -1;
		}
		c1 = src[1];
		if ((c1 & 0xC0u) != 0x80u) {
			return -1;
		}
		cp = ((uint32_t)(c0 & 0x1Fu) << 6) | (uint32_t)(c1 & 0x3Fu);
		if (cp < 0x80u) {
			return -1; /* overlong */
		}
		*out_cp = cp;
		return 2;
	}
	if ((c0 & 0xF0u) == 0xE0u) {
		unsigned char c1, c2;
		uint32_t cp;
		if (src_len < 3) {
			return -1;
		}
		c1 = src[1];
		c2 = src[2];
		if ((c1 & 0xC0u) != 0x80u || (c2 & 0xC0u) != 0x80u) {
			return -1;
		}
		cp = ((uint32_t)(c0 & 0x0Fu) << 12) |
			((uint32_t)(c1 & 0x3Fu) << 6) |
			(uint32_t)(c2 & 0x3Fu);
		if (cp < 0x800u) {
			return -1; /* overlong */
		}
		if (cp >= 0xD800u && cp <= 0xDFFFu) {
			return -1; /* UTF-16 surrogate */
		}
		*out_cp = cp;
		return 3;
	}
	if ((c0 & 0xF8u) == 0xF0u) {
		unsigned char c1, c2, c3;
		uint32_t cp;
		if (src_len < 4) {
			return -1;
		}
		c1 = src[1];
		c2 = src[2];
		c3 = src[3];
		if ((c1 & 0xC0u) != 0x80u || (c2 & 0xC0u) != 0x80u ||
			(c3 & 0xC0u) != 0x80u) {
			return -1;
		}
		cp = ((uint32_t)(c0 & 0x07u) << 18) |
			((uint32_t)(c1 & 0x3Fu) << 12) |
			((uint32_t)(c2 & 0x3Fu) << 6) |
			(uint32_t)(c3 & 0x3Fu);
		if (cp < 0x10000u || cp > 0x10FFFFu) {
			return -1;
		}
		*out_cp = cp;
		return 4;
	}
	return -1;
}

/* Encode Unicode codepoint to UTF-8. Returns bytes written, or -1. */
static inline int generals_utf8_encode(uint32_t cp, unsigned char *out, size_t out_cap)
{
	if (cp <= 0x7Fu) {
		if (out != NULL) {
			if (out_cap < 1) {
				return -1;
			}
			out[0] = (unsigned char)cp;
		}
		return 1;
	}
	if (cp <= 0x7FFu) {
		if (out != NULL) {
			if (out_cap < 2) {
				return -1;
			}
			out[0] = (unsigned char)(0xC0u | (cp >> 6));
			out[1] = (unsigned char)(0x80u | (cp & 0x3Fu));
		}
		return 2;
	}
	if (cp <= 0xFFFFu) {
		if (cp >= 0xD800u && cp <= 0xDFFFu) {
			return -1;
		}
		if (out != NULL) {
			if (out_cap < 3) {
				return -1;
			}
			out[0] = (unsigned char)(0xE0u | (cp >> 12));
			out[1] = (unsigned char)(0x80u | ((cp >> 6) & 0x3Fu));
			out[2] = (unsigned char)(0x80u | (cp & 0x3Fu));
		}
		return 3;
	}
	if (cp <= 0x10FFFFu) {
		if (out != NULL) {
			if (out_cap < 4) {
				return -1;
			}
			out[0] = (unsigned char)(0xF0u | (cp >> 18));
			out[1] = (unsigned char)(0x80u | ((cp >> 12) & 0x3Fu));
			out[2] = (unsigned char)(0x80u | ((cp >> 6) & 0x3Fu));
			out[3] = (unsigned char)(0x80u | (cp & 0x3Fu));
		}
		return 4;
	}
	return -1;
}

/* Append UTF-16 code units for a codepoint. Returns units written (1 or 2). */
static inline int generals_utf16_append(uint32_t cp, uint16_t *out, size_t out_cap)
{
	if (cp <= 0xFFFFu) {
		if (cp >= 0xD800u && cp <= 0xDFFFu) {
			return -1;
		}
		if (out != NULL) {
			if (out_cap < 1) {
				return -1;
			}
			out[0] = (uint16_t)cp;
		}
		return 1;
	}
	if (cp <= 0x10FFFFu) {
		uint32_t v = cp - 0x10000u;
		if (out != NULL) {
			if (out_cap < 2) {
				return -1;
			}
			out[0] = (uint16_t)(0xD800u + (v >> 10));
			out[1] = (uint16_t)(0xDC00u + (v & 0x3FFu));
		}
		return 2;
	}
	return -1;
}

/*
 * UTF-8 -> UTF-16. If dst is NULL, returns required unit count (excluding NUL
 * unless include_null). On invalid UTF-8, replaces with U+FFFD when possible.
 */
static inline int generals_utf8_to_utf16(
	const char *src,
	int srclen,
	uint16_t *dst,
	int dstlen,
	int include_null,
	int *had_error)
{
	const unsigned char *p;
	size_t remaining;
	int out = 0;
	int err = 0;

	if (src == NULL) {
		return 0;
	}
	if (srclen < 0) {
		srclen = 0;
		while (src[srclen] != '\0') {
			++srclen;
		}
	}
	p = (const unsigned char *)src;
	remaining = (size_t)srclen;

	while (remaining > 0) {
		uint32_t cp = 0;
		int n = generals_utf8_decode(p, remaining, &cp);
		int units;
		if (n < 0) {
			err = 1;
			cp = 0xFFFDu;
			n = 1;
		}
		units = generals_utf16_append(
			cp,
			(dst != NULL && out < dstlen) ? dst + out : NULL,
			(dst != NULL && out < dstlen) ? (size_t)(dstlen - out) : 0);
		if (units < 0) {
			err = 1;
			units = generals_utf16_append(
				0xFFFDu,
				(dst != NULL && out < dstlen) ? dst + out : NULL,
				(dst != NULL && out < dstlen) ? (size_t)(dstlen - out) : 0);
			if (units < 0) {
				units = 0;
			}
		}
		if (dst != NULL) {
			if (out + units > dstlen) {
				break;
			}
		}
		out += units;
		p += (size_t)n;
		remaining -= (size_t)n;
	}

	if (include_null) {
		if (dst != NULL) {
			if (out < dstlen) {
				dst[out++] = 0;
			} else if (dstlen > 0) {
				dst[dstlen - 1] = 0;
				out = dstlen;
			}
		} else {
			++out;
		}
	}
	if (had_error != NULL) {
		*had_error = err;
	}
	return out;
}

/* UTF-16 -> UTF-8. Same sizing convention as Win32 WideCharToMultiByte. */
static inline int generals_utf16_to_utf8(
	const uint16_t *src,
	int srclen,
	char *dst,
	int dstlen,
	int include_null,
	int *had_error)
{
	int i = 0;
	int out = 0;
	int err = 0;

	if (src == NULL) {
		return 0;
	}
	if (srclen < 0) {
		srclen = 0;
		while (src[srclen] != 0) {
			++srclen;
		}
	}

	while (i < srclen) {
		uint32_t cp;
		uint16_t u = src[i++];
		int n;
		if (u >= 0xD800u && u <= 0xDBFFu) {
			if (i < srclen) {
				uint16_t u2 = src[i];
				if (u2 >= 0xDC00u && u2 <= 0xDFFFu) {
					++i;
					cp = 0x10000u +
						(((uint32_t)(u - 0xD800u) << 10) |
						 (uint32_t)(u2 - 0xDC00u));
				} else {
					err = 1;
					cp = 0xFFFDu;
				}
			} else {
				err = 1;
				cp = 0xFFFDu;
			}
		} else if (u >= 0xDC00u && u <= 0xDFFFu) {
			err = 1;
			cp = 0xFFFDu;
		} else {
			cp = u;
		}

		n = generals_utf8_encode(
			cp,
			(dst != NULL && out < dstlen)
				? (unsigned char *)(dst + out)
				: NULL,
			(dst != NULL && out < dstlen) ? (size_t)(dstlen - out) : 0);
		if (n < 0) {
			err = 1;
			n = generals_utf8_encode(
				0xFFFDu,
				(dst != NULL && out < dstlen)
					? (unsigned char *)(dst + out)
					: NULL,
				(dst != NULL && out < dstlen) ? (size_t)(dstlen - out) : 0);
			if (n < 0) {
				n = 0;
			}
		}
		if (dst != NULL) {
			if (out + n > dstlen) {
				break;
			}
		}
		out += n;
	}

	if (include_null) {
		if (dst != NULL) {
			if (out < dstlen) {
				dst[out++] = '\0';
			} else if (dstlen > 0) {
				dst[dstlen - 1] = '\0';
				out = dstlen;
			}
		} else {
			++out;
		}
	}
	if (had_error != NULL) {
		*had_error = err;
	}
	return out;
}

#ifdef __cplusplus
}
#endif

#endif /* GENERALS_UTF8_CONVERT_H */
