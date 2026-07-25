/*
** Wide-string helpers for Linux builds compiled with -fshort-wchar.
** glibc wchar.h routines assume 4-byte wchar_t; game WCHAR is 2 bytes.
*/

#ifndef WW_WCSTRING_H
#define WW_WCSTRING_H

#include <stddef.h>

#if defined(RENEGADE_LINUX) || defined(GENERALS_LINUX)

static inline size_t WW_WCSTRLEN(const WCHAR *str)
{
	if (str == NULL) {
		return 0;
	}

	size_t n = 0;
	while (str[n] != 0) {
		++n;
	}
	return n;
}

static inline WCHAR *WW_WCSCPY(WCHAR *dst, const WCHAR *src)
{
	WCHAR *ret = dst;
	if (dst != NULL && src != NULL) {
		while ((*dst = *src) != 0) {
			++dst;
			++src;
		}
		*dst = 0;
	}
	return ret;
}

static inline const WCHAR *WW_WCSSTR(const WCHAR *haystack, const WCHAR *needle)
{
	if (haystack == NULL || needle == NULL) {
		return NULL;
	}
	if (needle[0] == 0) {
		return haystack;
	}

	for (; haystack[0] != 0; ++haystack) {
		const WCHAR *h = haystack;
		const WCHAR *n = needle;
		while (h[0] != 0 && n[0] != 0 && h[0] == n[0]) {
			++h;
			++n;
		}
		if (n[0] == 0) {
			return haystack;
		}
	}

	return NULL;
}

static inline WCHAR *WW_WCSCHR(const WCHAR *str, WCHAR ch)
{
	if (str == NULL) {
		return NULL;
	}
	for (; *str != 0; ++str) {
		if (*str == ch) {
			return const_cast<WCHAR *>(str);
		}
	}
	return (ch == 0) ? const_cast<WCHAR *>(str) : NULL;
}

static inline WCHAR *WW_WCSCAT(WCHAR *dst, const WCHAR *src)
{
	if (dst == NULL) {
		return dst;
	}
	WCHAR *ret = dst;
	while (*dst != 0) {
		++dst;
	}
	if (src != NULL) {
		while ((*dst = *src) != 0) {
			++dst;
			++src;
		}
	}
	return ret;
}

static inline int WW_WCSCMP(const WCHAR *lhs, const WCHAR *rhs)
{
	if (lhs == rhs) {
		return 0;
	}
	if (lhs == NULL) {
		return -1;
	}
	if (rhs == NULL) {
		return 1;
	}

	while (lhs[0] != 0 && rhs[0] != 0 && lhs[0] == rhs[0]) {
		++lhs;
		++rhs;
	}
	return (int)lhs[0] - (int)rhs[0];
}

static inline int WW_WCSNCMP(const WCHAR *lhs, const WCHAR *rhs, size_t count)
{
	if (count == 0) {
		return 0;
	}
	if (lhs == NULL && rhs == NULL) {
		return 0;
	}
	if (lhs == NULL) {
		return -1;
	}
	if (rhs == NULL) {
		return 1;
	}

	for (size_t i = 0; i < count; ++i) {
		if (lhs[i] != rhs[i]) {
			return (int)lhs[i] - (int)rhs[i];
		}
		if (lhs[i] == 0) {
			break;
		}
	}
	return 0;
}

static inline WCHAR *WW_WCSNCPY(WCHAR *dst, const WCHAR *src, size_t count)
{
	if (dst == NULL || count == 0) {
		return dst;
	}

	WCHAR *ret = dst;
	size_t i = 0;
	if (src != NULL) {
		for (; i < count && src[i] != 0; ++i) {
			dst[i] = src[i];
		}
	}
	for (; i < count; ++i) {
		dst[i] = 0;
	}
	return ret;
}

static inline WCHAR WW_WCHAR_TOUPPER(WCHAR ch)
{
	if (ch >= (WCHAR)L'a' && ch <= (WCHAR)L'z') {
		return (WCHAR)(ch - ((WCHAR)L'a' - (WCHAR)L'A'));
	}
	return ch;
}

static inline WCHAR *WW_WCSUPR(WCHAR *str)
{
	if (str != NULL) {
		for (WCHAR *p = str; *p != 0; ++p) {
			*p = WW_WCHAR_TOUPPER(*p);
		}
	}
	return str;
}

static inline int WW_WCSICMP(const WCHAR *lhs, const WCHAR *rhs)
{
	if (lhs == rhs) {
		return 0;
	}
	if (lhs == NULL) {
		return -1;
	}
	if (rhs == NULL) {
		return 1;
	}

	while (lhs[0] != 0 && rhs[0] != 0) {
		const WCHAR la = WW_WCHAR_TOUPPER(lhs[0]);
		const WCHAR rb = WW_WCHAR_TOUPPER(rhs[0]);
		if (la != rb) {
			return (int)la - (int)rb;
		}
		++lhs;
		++rhs;
	}

	return (int)WW_WCHAR_TOUPPER(lhs[0]) - (int)WW_WCHAR_TOUPPER(rhs[0]);
}

static inline WCHAR *WW_WMEMMOVE(WCHAR *dst, const WCHAR *src, size_t n)
{
	if (dst == NULL || src == NULL || n == 0) {
		return dst;
	}
	if (dst == src) {
		return dst;
	}
	if (dst < src) {
		for (size_t i = 0; i < n; ++i) {
			dst[i] = src[i];
		}
	} else {
		for (size_t i = n; i > 0; --i) {
			dst[i - 1] = src[i - 1];
		}
	}
	return dst;
}

static inline int WW_ISWSPACE(WCHAR ch)
{
	return ch == (WCHAR)L' ' || ch == (WCHAR)L'\t' || ch == (WCHAR)L'\n'
		|| ch == (WCHAR)L'\r' || ch == (WCHAR)L'\v' || ch == (WCHAR)L'\f';
}

#include "ww_wcstring_macros.h"

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif
int WW_VSNWPRINTF(WCHAR *buffer, size_t count, const WCHAR *format, va_list args);
#ifdef __cplusplus
}
#endif

#endif /* RENEGADE_LINUX */

#endif /* WW_WCSTRING_H */
