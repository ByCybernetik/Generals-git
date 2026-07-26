/* Minimal RenderDoc app API stub for Linux builds (optional capture). */
#ifndef RENDERDOC_APP_H
#define RENDERDOC_APP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum RENDERDOC_Version {
	eRENDERDOC_API_Version_1_0_0 = 10000
} RENDERDOC_Version;

typedef struct RENDERDOC_API_1_0_0 {
	void (*TriggerCapture)(void);
} RENDERDOC_API_1_0_0;

typedef int (*pRENDERDOC_GetAPI)(RENDERDOC_Version version, void **outAPI);

#ifdef __cplusplus
}
#endif

#endif /* RENDERDOC_APP_H */
