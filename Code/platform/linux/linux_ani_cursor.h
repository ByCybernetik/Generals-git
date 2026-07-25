#pragma once

#include "linux/win32_minimal.h"

#ifdef __cplusplus
extern "C" {
#endif

HCURSOR Linux_Load_Cursor_From_FileA(LPCSTR fileName);
HCURSOR Linux_Set_Cursor(HCURSOR hCursor);
void Linux_Ani_Cursor_Tick_Active(void);
void Linux_Ani_Cursor_Shutdown(void);

#ifdef __cplusplus
}
#endif
