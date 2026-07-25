/*
** Re-apply 16-bit wchar libc macro overrides after STL <string>/<cwchar> headers.
** Intentionally has no include guard; safe to include multiple times per TU.
*/

#if defined(RENEGADE_LINUX) || defined(GENERALS_LINUX)

#undef wcslen
#define wcslen WW_WCSTRLEN
#undef wcscpy
#define wcscpy WW_WCSCPY
#undef wcscat
#define wcscat WW_WCSCAT
#undef wcsstr
#define wcsstr WW_WCSSTR
#undef wcschr
#define wcschr WW_WCSCHR
#undef wcscmp
#define wcscmp WW_WCSCMP
#undef wcsncmp
#define wcsncmp WW_WCSNCMP
#undef wcsncpy
#define wcsncpy WW_WCSNCPY
#undef wmemmove
#define wmemmove WW_WMEMMOVE
#undef iswspace
#define iswspace WW_ISWSPACE
#undef _vsnwprintf
#define _vsnwprintf WW_VSNWPRINTF
#undef _wcsicmp
#define _wcsicmp WW_WCSICMP
#undef wcsicmp
#define wcsicmp WW_WCSICMP
#undef wcscasecmp
#define wcscasecmp WW_WCSICMP
#undef _wcsupr
#define _wcsupr WW_WCSUPR

#endif /* RENEGADE_LINUX || GENERALS_LINUX */
