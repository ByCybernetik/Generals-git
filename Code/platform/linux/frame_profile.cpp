/*
 * Accumulates per-stage frame times and dumps averages every ~2 seconds.
 */
#if defined(GENERALS_LINUX) || defined(RENEGADE_LINUX)

#include "frame_profile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace {

struct StageAccum {
	const char *name;
	double totalMs;
	double peakMs;
};

enum {
	STAGE_AUDIO = 0,
	STAGE_CLIENT,
	STAGE_NETWORK,
	STAGE_LOGIC,
	STAGE_COUNT
};

static bool g_enabled = false;
static FILE *g_log = NULL;
static StageAccum g_stages[STAGE_COUNT];
static double g_updateTotalMs = 0.0;
static double g_waitTotalMs = 0.0;
static double g_peakUpdateMs = 0.0;
static double g_lastUpdateMs = 0.0;
static int g_frames = 0; /* visual outer-loop frames */
static int g_logicTicks = 0;
static int g_visualFrames = 0;
static timespec g_frameStart;
static timespec g_markStart;
static int g_curStage = -1;
static timespec g_lastDump;

static double timespec_diff_ms(const timespec &a, const timespec &b)
{
	return (double)(b.tv_sec - a.tv_sec) * 1000.0 +
		(double)(b.tv_nsec - a.tv_nsec) / 1000000.0;
}

static void flush_stage()
{
	if (g_curStage < 0 || g_curStage >= STAGE_COUNT) {
		return;
	}
	timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	const double ms = timespec_diff_ms(g_markStart, now);
	g_stages[g_curStage].totalMs += ms;
	if (ms > g_stages[g_curStage].peakMs) {
		g_stages[g_curStage].peakMs = ms;
	}
	g_markStart = now;
}

static int stage_index(const char *name)
{
	if (strcmp(name, "audio") == 0) return STAGE_AUDIO;
	if (strcmp(name, "client") == 0) return STAGE_CLIENT;
	if (strcmp(name, "network") == 0) return STAGE_NETWORK;
	if (strcmp(name, "logic") == 0) return STAGE_LOGIC;
	return -1;
}

static void dump_window()
{
	if (!g_log || g_frames <= 0) {
		return;
	}
	const double inv = 1.0 / (double)g_frames;
	const double avgUpdate = g_updateTotalMs * inv;
	const double avgWait = g_waitTotalMs * inv;
	const double avgLoop = avgUpdate + avgWait;
	const double avgFps = avgLoop > 0.01 ? (1000.0 / avgLoop) : 0.0;
	const double logicHz = (double)g_logicTicks / 2.0; /* dump window ~2s */
	const double visualHz = (double)g_visualFrames / 2.0;
	fprintf(g_log,
		"visual=%d logicTicks=%d (~%.1f logicHz, ~%.1f visualHz) avgUpdate=%.2fms peakUpdate=%.2fms avgWait=%.2fms avgLoop=%.2fms (%.1f FPS) | "
		"audio=%.2f (pk %.2f) client=%.2f (pk %.2f) network=%.2f (pk %.2f) logic=%.2f (pk %.2f)\n",
		g_frames, g_logicTicks, logicHz, visualHz,
		avgUpdate, g_peakUpdateMs, avgWait, avgLoop, avgFps,
		g_stages[STAGE_AUDIO].totalMs * inv, g_stages[STAGE_AUDIO].peakMs,
		g_stages[STAGE_CLIENT].totalMs * inv, g_stages[STAGE_CLIENT].peakMs,
		g_stages[STAGE_NETWORK].totalMs * inv, g_stages[STAGE_NETWORK].peakMs,
		g_stages[STAGE_LOGIC].totalMs * inv, g_stages[STAGE_LOGIC].peakMs);
	fflush(g_log);

	for (int i = 0; i < STAGE_COUNT; ++i) {
		g_stages[i].totalMs = 0.0;
		g_stages[i].peakMs = 0.0;
	}
	g_updateTotalMs = 0.0;
	g_waitTotalMs = 0.0;
	g_peakUpdateMs = 0.0;
	g_frames = 0;
	g_logicTicks = 0;
	g_visualFrames = 0;
}

} // namespace

extern "C" void FrameProfile_Init(void)
{
	const char *env = getenv("GENERALS_PROFILE");
	g_enabled = (env != NULL && env[0] != '\0' && strcmp(env, "0") != 0);
	if (!g_enabled) {
		return;
	}

	g_stages[STAGE_AUDIO].name = "audio";
	g_stages[STAGE_CLIENT].name = "client";
	g_stages[STAGE_NETWORK].name = "network";
	g_stages[STAGE_LOGIC].name = "logic";

	g_log = fopen("fps_profile.log", "w");
	if (!g_log) {
		g_log = fopen("/tmp/generals_fps_profile.log", "w");
	}
	if (g_log) {
		fprintf(g_log, "# Generals Linux frame profile (GENERALS_PROFILE=1)\n");
		fprintf(g_log, "# logic ~30Hz; visual = FramesPerSecondLimit; wait = visual FPS Sleep\n");
		fflush(g_log);
	}
	clock_gettime(CLOCK_MONOTONIC, &g_lastDump);
}

extern "C" void FrameProfile_BeginFrame(void)
{
	if (!g_enabled) {
		return;
	}
	clock_gettime(CLOCK_MONOTONIC, &g_frameStart);
	g_markStart = g_frameStart;
	g_curStage = -1;
}

extern "C" void FrameProfile_Mark(const char *stage)
{
	if (!g_enabled || stage == NULL) {
		return;
	}
	flush_stage();
	g_curStage = stage_index(stage);
	clock_gettime(CLOCK_MONOTONIC, &g_markStart);
}

extern "C" void FrameProfile_EndUpdate(void)
{
	if (!g_enabled) {
		return;
	}
	flush_stage();
	g_curStage = -1;

	timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	g_lastUpdateMs = timespec_diff_ms(g_frameStart, now);
	g_updateTotalMs += g_lastUpdateMs;
	if (g_lastUpdateMs > g_peakUpdateMs) {
		g_peakUpdateMs = g_lastUpdateMs;
	}
}

extern "C" void FrameProfile_NoteLogicTick(void)
{
	if (!g_enabled) {
		return;
	}
	++g_logicTicks;
}

extern "C" void FrameProfile_NoteVisualFrame(void)
{
	if (!g_enabled) {
		return;
	}
	++g_visualFrames;
}

extern "C" void FrameProfile_EndFrame(double fpsWaitMs)
{
	if (!g_enabled) {
		return;
	}
	g_waitTotalMs += fpsWaitMs;
	++g_frames;

	timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	if (timespec_diff_ms(g_lastDump, now) >= 2000.0) {
		dump_window();
		g_lastDump = now;
	}
}

extern "C" void FrameProfile_Shutdown(void)
{
	if (!g_enabled) {
		return;
	}
	dump_window();
	if (g_log) {
		fclose(g_log);
		g_log = NULL;
	}
	g_enabled = false;
}

#endif
