/*
 * Lightweight frame-stage profiler for Linux Generals builds.
 * Enable with env GENERALS_PROFILE=1 (writes game/fps_profile.log).
 */
#ifndef GENERALS_FRAME_PROFILE_H
#define GENERALS_FRAME_PROFILE_H

#if defined(GENERALS_LINUX) || defined(RENEGADE_LINUX)

#ifdef __cplusplus
extern "C" {
#endif

void FrameProfile_Init(void);
void FrameProfile_BeginFrame(void);
void FrameProfile_Mark(const char *stage);
/* Call at end of GameEngine::update — records stage + update duration. */
void FrameProfile_EndUpdate(void);
void FrameProfile_NoteLogicTick(void);
void FrameProfile_NoteVisualFrame(void);
/* Call after FPS limiter Sleep — waitMs and dump window. */
void FrameProfile_EndFrame(double fpsWaitMs);
void FrameProfile_Shutdown(void);

#ifdef __cplusplus
}
#endif

#define FRAME_PROFILE_BEGIN() FrameProfile_BeginFrame()
#define FRAME_PROFILE_MARK(s) FrameProfile_Mark(s)
#define FRAME_PROFILE_END_UPDATE() FrameProfile_EndUpdate()
#define FRAME_PROFILE_END(w) FrameProfile_EndFrame(w)

#else

#define FRAME_PROFILE_BEGIN() ((void)0)
#define FRAME_PROFILE_MARK(s) ((void)0)
#define FRAME_PROFILE_END_UPDATE() ((void)0)
#define FRAME_PROFILE_END(w) ((void)0)
static inline void FrameProfile_NoteLogicTick(void) {}
static inline void FrameProfile_NoteVisualFrame(void) {}

#endif

#endif
