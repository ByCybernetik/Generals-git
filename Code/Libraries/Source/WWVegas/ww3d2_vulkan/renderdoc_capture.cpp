#include "renderdoc_capture.h"

#if defined(RENEGADE_LINUX) || defined(GENERALS_LINUX)

#include <dlfcn.h>
#include <cstdio>

#define RENDERDOC_HEADERS_NO_SYSTEM
#include <renderdoc_app.h>

namespace ww3d_vulkan {

namespace {

pRENDERDOC_GetAPI g_get_api = nullptr;
RENDERDOC_API_1_0_0 *g_api = nullptr;
bool g_initialized = false;
bool g_triggered = false;

void Ensure_Init()
{
	if (g_initialized) {
		return;
	}
	g_initialized = true;

	/*
	 * librenderdoc.so is only present when the process was launched under
	 * RenderDoc.  dlopen fails harmlessly otherwise.
	 */
	void *handle = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD);
	if (handle == nullptr) {
		return;
	}
	g_get_api = reinterpret_cast<pRENDERDOC_GetAPI>(
		dlsym(handle, "RENDERDOC_GetAPI"));
	if (g_get_api == nullptr) {
		return;
	}
	int ok = g_get_api(eRENDERDOC_API_Version_1_0_0,
		reinterpret_cast<void **>(&g_api));
	if (ok != 1) {
		g_api = nullptr;
	}
}

} /* namespace */

bool RenderDoc_Available()
{
	Ensure_Init();
	return g_api != nullptr;
}

void RenderDoc_Trigger_Once()
{
	Ensure_Init();
	if (g_api == nullptr || g_triggered) {
		return;
	}
	g_triggered = true;

	/*
	 * TriggerCapture asks RenderDoc to grab the next frame boundary (the
	 * present after this call).  Because this is called from inside a frame
	 * that is already recording shadow volumes, the captured frame will
	 * contain the full stencil shadow pass.
	 */
	g_api->TriggerCapture();
	fprintf(stderr, "renderdoc: triggered capture of next frame\n");
}

} /* namespace ww3d_vulkan */

#endif
