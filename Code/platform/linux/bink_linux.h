/*
 * Bink API shim for Generals Linux (FFmpeg backend in bink_ffmpeg.cpp).
 */
#ifndef BINK_LINUX_H
#define BINK_LINUX_H

typedef struct BINK {
	unsigned long Width;
	unsigned long Height;
	unsigned long Frames;
	unsigned long FrameNum;
	unsigned long FrameRate;
	unsigned long FrameRateDiv;
} BINK;

typedef BINK *HBINK;

/* Match Generals / RAD surface flags used by BinkVideoPlayer.cpp */
#define BINKSURFACE32   1u
#define BINKSURFACE24   2u
#define BINKSURFACE565  0x00008000u
#define BINKSURFACE555  0x00004000u
#define BINKCOPYNOSCALING 0x00000001u

#define BINKPRELOADALL  0x00080000u

typedef unsigned int u32;

#ifdef __cplusplus
extern "C" {
#endif

long BinkSoundUseDirectSound(void *driver);
void BinkSetSoundTrack(unsigned long, unsigned long);
void BinkSetVolume(HBINK bink, unsigned long track, long volume);
HBINK BinkOpen(const char *name, unsigned long flags);
void BinkClose(HBINK bink);
long BinkWait(HBINK bink);
void BinkDoFrame(HBINK bink);
void BinkCopyToBuffer(HBINK bink, void *dest, long destpitch, unsigned long destheight,
	unsigned long destx, unsigned long desty, unsigned long flags);
void BinkNextFrame(HBINK bink);
void BinkGoto(HBINK bink, unsigned long frame, void *reserved);

#ifdef __cplusplus
}
#endif

#endif
