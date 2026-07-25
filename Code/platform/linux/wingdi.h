#ifndef RENEGADE_WINGDI_H
#define RENEGADE_WINGDI_H

#include <windows.h>

typedef HANDLE HFONT;
typedef HANDLE HBITMAP;
typedef HANDLE HDC;
typedef HANDLE HGDIOBJ;

#ifndef SRCCOPY
#define SRCCOPY 0x00CC0020
#endif
#ifndef IMAGE_BITMAP
#define IMAGE_BITMAP 0
#endif
#ifndef LR_SHARED
#define LR_SHARED 0x8000
#endif
#ifndef LR_LOADFROMFILE
#define LR_LOADFROMFILE 0x0010
#endif

typedef struct tagPAINTSTRUCT {
	HDC hdc;
	BOOL fErase;
	RECT rcPaint;
	BOOL fRestore;
	BOOL fIncUpdate;
	BYTE rgbReserved[32];
} PAINTSTRUCT, *LPPAINTSTRUCT;

#ifdef __cplusplus
extern "C" {
#endif

HDC BeginPaint(HWND hwnd, LPPAINTSTRUCT paint);
BOOL EndPaint(HWND hwnd, const PAINTSTRUCT *paint);
HDC CreateCompatibleDC(HDC hdc);
HGDIOBJ SelectObject(HDC hdc, HGDIOBJ obj);
BOOL BitBlt(HDC hdcDest, int x, int y, int cx, int cy, HDC hdcSrc, int x1, int y1, DWORD rop);
BOOL DeleteDC(HDC hdc);
int SaveDC(HDC hdc);
BOOL RestoreDC(HDC hdc, int saved);
BOOL DeleteObject(HGDIOBJ obj);
HANDLE LoadImageA(HINSTANCE inst, LPCSTR name, UINT type, int cx, int cy, UINT flags);
#define LoadImage LoadImageA

#ifdef __cplusplus
}
#endif

#endif
