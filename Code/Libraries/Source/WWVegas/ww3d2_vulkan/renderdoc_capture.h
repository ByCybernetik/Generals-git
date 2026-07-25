/*
 * Minimal RenderDoc inline-capture integration.
 *
 * When the process is running under RenderDoc (librenderdoc.so is present and
 * RENDERDOC_GetAPI succeeds), the capture helper can trigger a single-frame
 * capture at a deterministic point — e.g. as soon as the first volumetric
 * shadow-volume draw is recorded — so the resulting .rdc contains a frame
 * with the stencil shadow passes.
 *
 * Outside RenderDoc the helpers are inert no-ops.
 */
#ifndef WW3D2_VULKAN_RENDERDOC_CAPTURE_H
#define WW3D2_VULKAN_RENDERDOC_CAPTURE_H

#include <vulkan/vulkan.h>

#if defined(RENEGADE_LINUX) || defined(GENERALS_LINUX)

namespace ww3d_vulkan {

/*
 * Returns true if RenderDoc is attached and its API could be loaded.  Safe to
 * call before VkInstance exists; the lookup is cached after the first call.
 */
bool RenderDoc_Available();

/*
 * Trigger a capture of the next frame boundary, at most once per process.
 * Intended to be called from the draw path that must appear in the capture
 * (e.g. Draw_Shadow_Volume).  Subsequent calls are no-ops.
 */
void RenderDoc_Trigger_Once();

} /* namespace ww3d_vulkan */

#else

namespace ww3d_vulkan {
inline bool RenderDoc_Available() { return false; }
inline void RenderDoc_Trigger_Once() {}
} /* namespace ww3d_vulkan */

#endif

#endif /* WW3D2_VULKAN_RENDERDOC_CAPTURE_H */
