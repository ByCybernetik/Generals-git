/* Minimal Bink SDK stub for Linux builds without RAD Game Tools libraries. */
#pragma once

#ifndef BINK_LINUX_STUB_H
#define BINK_LINUX_STUB_H

typedef void *HBINK;
typedef void *HBINKBUFFER;

#define BINKSURFACE32 1

struct BINKSUMMARY
{
	unsigned Frames;
	unsigned Width;
	unsigned Height;
};

inline HBINK BinkOpen(const char *, unsigned) { return nullptr; }
inline void BinkClose(HBINK) {}
inline int BinkWait(HBINK) { return 0; }
inline void BinkDoFrame(HBINK) {}
inline void BinkNextFrame(HBINK) {}
inline void BinkCopyToBuffer(HBINK, void *, unsigned, unsigned, unsigned, unsigned, unsigned) {}
inline void BinkGetSummary(HBINK, BINKSUMMARY *) {}

#endif
