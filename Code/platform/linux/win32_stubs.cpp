/*
 * Stubs for Win32 APIs used before full port coverage (no real window class on Linux).
 */
#include "win32_minimal.h"
#include "wingdi.h"
#include "sdl3_host.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

HWND GetDesktopWindow(void) { return Platform_Get_Main_HWnd(); }
BOOL SetFocus(HWND) { return TRUE; }

BOOL UpdateWindow(HWND) { return TRUE; }

int MessageBoxA(HWND, LPCSTR text, LPCSTR caption, UINT)
{
	fprintf(stderr, "%s: %s\n", caption ? caption : "Message", text ? text : "");
	return 0;
}

BOOL RegisterClassA(const WNDCLASS *) { return TRUE; }

HWND CreateWindowExA(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int,
	HWND, HANDLE, HINSTANCE, LPVOID)
{
	/* SDL window is created in Platform_Init_Video_Audio; do not move/resize here (Wayland). */
	return Platform_Get_Main_HWnd();
}

BOOL ValidateRect(HWND, const RECT *) { return TRUE; }

BOOL GetWindowRect(HWND, RECT *rect)
{
	if (rect == NULL) {
		return FALSE;
	}
	SDL_Window *win = Platform_Get_SDL_Window();
	if (win != NULL) {
		int wx = 0;
		int wy = 0;
		int w = 0;
		int h = 0;
		SDL_GetWindowPosition(win, &wx, &wy);
		SDL_GetWindowSize(win, &w, &h);
		rect->left = wx;
		rect->top = wy;
		rect->right = wx + w;
		rect->bottom = wy + h;
	} else {
		rect->left = rect->top = 0;
		rect->right = 800;
		rect->bottom = 600;
	}
	return TRUE;
}

BOOL ClipCursor(const RECT *rect)
{
	SDL_Window *win = Platform_Get_SDL_Window();
	if (win == NULL) {
		return FALSE;
	}
	if (rect == NULL) {
		return SDL_SetWindowMouseRect(win, NULL) ? TRUE : FALSE;
	}

	int w = 0;
	int h = 0;
	if (!SDL_GetWindowSizeInPixels(win, &w, &h) || w <= 0 || h <= 0) {
		SDL_GetWindowSize(win, &w, &h);
	}
	if (w <= 0 || h <= 0) {
		return FALSE;
	}

	const SDL_Rect clip = { 0, 0, w, h };
	return SDL_SetWindowMouseRect(win, &clip) ? TRUE : FALSE;
}

BOOL AdjustWindowRect(RECT *rect, DWORD, BOOL)
{
	return rect != NULL;
}

int GetSystemMetrics(int index)
{
	switch (index) {
	case 0: /* SM_CXSCREEN */
	case 1: /* SM_CYSCREEN */
	{
		SDL_DisplayID did = SDL_GetPrimaryDisplay();
		if (did != 0) {
			SDL_Rect bounds;
			if (SDL_GetDisplayBounds(did, &bounds)) {
				return (index == 0) ? bounds.w : bounds.h;
			}
		}
		return (index == 0) ? 1920 : 1080;
	}
	default:
		return 0;
	}
}

LRESULT DefWindowProcA(HWND, UINT, WPARAM, LPARAM) { return 0; }
void ReleaseCapture(void) {}
extern "C" void Renegade_Stop_Main_Loop(int exitCode);

void PostQuitMessage(int exitCode)
{
	Renegade_Stop_Main_Loop(exitCode);
}

BOOL PeekMessageA(MSG *msg, HWND, UINT, UINT, UINT remove)
{
	(void)remove;
	if (!msg) {
		return FALSE;
	}
	memset(msg, 0, sizeof(*msg));
	return FALSE;
}

BOOL GetMessageA(MSG *msg, HWND, UINT, UINT)
{
	(void)msg;
	Platform_Pump_Events();
	return FALSE;
}

BOOL TranslateMessage(const MSG *) { return TRUE; }
LRESULT DispatchMessageA(const MSG *) { return 0; }
HACCEL LoadAcceleratorsA(HMODULE, LPCSTR) { return NULL; }
BOOL TranslateAcceleratorA(HWND, HACCEL, MSG *) { return FALSE; }
BOOL IsDialogMessageA(HWND, MSG *) { return FALSE; }

SHORT GetAsyncKeyState(int vkey)
{
	return Platform_Get_Async_Key(vkey) ? (SHORT)0x8000 : 0;
}

SHORT GetKeyState(int vkey)
{
	/* Toggle keys: low bit = latched on; high bit = currently down. */
	if (vkey == VK_CAPITAL) {
		return Platform_Get_Async_Key(vkey) ? (SHORT)0x0001 : 0;
	}
	return GetAsyncKeyState(vkey);
}

void Sleep(DWORD ms) { SDL_Delay(ms); }

BOOL SearchPathA(LPCSTR path, LPCSTR file, LPCSTR, DWORD buflen, CHAR *out, CHAR **filepart)
{
	if (!file || !out || buflen < 2) {
		return FALSE;
	}
	const char *base = (path && path[0]) ? path : ".";
	snprintf(out, buflen, "%s/%s", base, file);
	for (char *p = out; *p; ++p) {
		if (*p == '\\') {
			*p = '/';
		}
	}
	struct stat st;
	if (stat(out, &st) != 0 || !S_ISREG(st.st_mode)) {
		return FALSE;
	}
	if (filepart) {
		char *slash = strrchr(out, '/');
		*filepart = slash ? slash + 1 : out;
	}
	return TRUE;
}

BOOL CreateProcessA(LPCSTR, LPSTR, LPVOID, LPVOID, BOOL, DWORD,
	LPVOID, LPCSTR, STARTUPINFO *, PROCESS_INFORMATION *)
{
	return FALSE;
}

HICON LoadIconA(HINSTANCE, LPCSTR) { return NULL; }
HCURSOR LoadCursorA(HINSTANCE, LPCSTR) { return NULL; }
HBRUSH GetStockObject(int) { return NULL; }
void SetUnhandledExceptionFilter(void *) {}

HDC BeginPaint(HWND, PAINTSTRUCT *paint)
{
	if (paint != NULL) {
		memset(paint, 0, sizeof(*paint));
	}
	return (HDC)1;
}

BOOL EndPaint(HWND, const PAINTSTRUCT *) { return TRUE; }

BOOL BitBlt(HDC, int, int, int, int, HDC, int, int, DWORD) { return TRUE; }

int SaveDC(HDC) { return 1; }

BOOL RestoreDC(HDC, int) { return TRUE; }

HANDLE LoadImageA(HINSTANCE, LPCSTR, UINT, int, int, UINT) { return NULL; }

#ifdef __cplusplus
}
#endif
