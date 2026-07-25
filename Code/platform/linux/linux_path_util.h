#pragma once

#if defined(GENERALS_LINUX) || defined(RENEGADE_LINUX)

#include <string.h>
#include "Common/AsciiString.h"

static inline void RenegadeNormalizePathSeparators(char *path)
{
	if (path == NULL) {
		return;
	}
	for (char *p = path; *p; ++p) {
		if (*p == '\\') {
			*p = '/';
		}
	}
}

static inline void RenegadeNormalizeAsciiPath(AsciiString &path)
{
	char buf[_MAX_PATH * 2];
	const char *src = path.str();
	if (src == NULL) {
		return;
	}
	strncpy(buf, src, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';
	RenegadeNormalizePathSeparators(buf);
	path = buf;
}

static inline const char *RenegadePathLastSep(const char *path)
{
	const char *bs = (path != NULL) ? strrchr(path, '\\') : NULL;
	const char *fs = (path != NULL) ? strrchr(path, '/') : NULL;
	if (bs == NULL) {
		return fs;
	}
	if (fs == NULL) {
		return bs;
	}
	return (bs > fs) ? bs : fs;
}

#endif
