#ifndef RENEGADE_WINUSER_EXTRA_H
#define RENEGADE_WINUSER_EXTRA_H

#include <windows.h>

#ifndef VK_LBUTTON
#define VK_LBUTTON 0x01
#define VK_RBUTTON 0x02
#define VK_CANCEL 0x03
#define VK_MBUTTON 0x04
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_CLEAR 0x0C
#define VK_RETURN 0x0D
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_PAUSE 0x13
#define VK_CAPITAL 0x14
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_SELECT 0x29
#define VK_PRINT 0x2A
#define VK_EXECUTE 0x2B
#define VK_SNAPSHOT 0x2C
#define VK_INSERT 0x2D
#define VK_DELETE 0x2E
#define VK_HELP 0x2F
#define VK_F1 0x70
#define VK_F2 0x71
#define VK_F3 0x72
#define VK_F4 0x73
#define VK_F5 0x74
#define VK_F6 0x75
#define VK_F7 0x76
#define VK_F8 0x77
#define VK_F9 0x78
#define VK_F10 0x79
#define VK_F11 0x7A
#define VK_F12 0x7B
#endif

#ifndef WM_SYSCOMMAND
#define WM_SYSCOMMAND 0x0112
#endif
#ifndef WM_CLOSE
#define WM_CLOSE 0x0010
#endif
#ifndef WM_SETFOCUS
#define WM_SETFOCUS 0x0007
#endif
#ifndef WM_KILLFOCUS
#define WM_KILLFOCUS 0x0008
#endif
#ifndef WM_SIZE
#define WM_SIZE 0x0005
#endif
#ifndef WM_ACTIVATE
#define WM_ACTIVATE 0x0006
#endif
#ifndef SIZE_RESTORED
#define SIZE_RESTORED 0
#endif
#ifndef MAKELPARAM
#define MAKELPARAM(l, h) ((LPARAM)((((DWORD)(h)) << 16) | ((WORD)(l))))
#endif
#ifndef WM_SETCURSOR
#define WM_SETCURSOR 0x0020
#endif
#ifndef WM_NCHITTEST
#define WM_NCHITTEST 0x0084
#endif
#ifndef WM_POWERBROADCAST
#define WM_POWERBROADCAST 0x0218
#endif
#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL 0x020A
#endif
#ifndef SC_CLOSE
#define SC_CLOSE 0xF060
#endif
#ifndef SC_MOVE
#define SC_MOVE 0xF010
#define SC_SIZE 0xF000
#define SC_MAXIMIZE 0xF030
#define SC_MONITORPOWER 0xF170
#endif
#ifndef SC_KEYMENU
#define SC_KEYMENU 0xF100
#endif
#ifndef SC_SCREENSAVE
#define SC_SCREENSAVE 0xF140
#endif
#ifndef WM_COMMAND
#define WM_COMMAND 0x0111
#endif

#ifndef IDOK
#define IDOK 1
#define IDCANCEL 2
#define IDABORT 3
#define IDRETRY 4
#define IDIGNORE 5
#define IDYES 6
#define IDNO 7
#endif

#ifndef MB_ABORTRETRYIGNORE
#define MB_ABORTRETRYIGNORE 0x00000002L
#define MB_TASKMODAL 0x00002000L
#define MB_SYSTEMMODAL 0x00001000L
#endif

#ifndef HWND_NOTOPMOST
#define HWND_NOTOPMOST ((HWND)(LONG_PTR)-2)
#define HWND_TOPMOST ((HWND)(LONG_PTR)-1)
#define HWND_TOP ((HWND)0)
#endif
#ifndef HTCLIENT
#define HTCLIENT 1
#endif
#ifndef WA_INACTIVE
#define WA_INACTIVE 0
#define WA_ACTIVE 1
#define WA_CLICKACTIVE 2
#endif
#ifndef SM_CXSCREEN
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#endif
#ifndef WS_POPUP
#define WS_POPUP 0x80000000L
#define WS_DLGFRAME 0x00400000L
#endif
#ifndef WS_EX_TOPMOST
#define WS_EX_TOPMOST 0x00000008L
#endif
#ifndef SWP_NOSIZE
#define SWP_NOSIZE 0x0001
#define SWP_NOMOVE 0x0002
#endif

#ifndef GMEM_ZEROINIT
#define GMEM_FIXED 0x0000
#define GMEM_ZEROINIT 0x0040
#endif

#ifndef RENEGADE_HGLOBAL_DEFINED
#define RENEGADE_HGLOBAL_DEFINED
typedef HANDLE HGLOBAL;
#endif

#ifndef DATE_SHORTDATE
#define DATE_SHORTDATE 0x00000001L
#endif
#ifndef TIME_NOSECONDS
#define TIME_NOSECONDS 0x00000002L
#endif
#ifndef TIME_NOTIMEMARKER
#define TIME_NOTIMEMARKER 0x00000004L
#endif

struct _EXCEPTION_RECORD;
struct _CONTEXT;
typedef struct _EXCEPTION_POINTERS {
	struct _EXCEPTION_RECORD *ExceptionRecord;
	struct _CONTEXT *ContextRecord;
} EXCEPTION_POINTERS, *LPEXCEPTION_POINTERS;

typedef void *HKL;
typedef DWORD LCID;
typedef DWORD LCTYPE;

struct _SYSTEMTIME;
typedef struct _SYSTEMTIME SYSTEMTIME;

#ifdef __cplusplus
extern "C" {
#endif

void DebugBreak(void);
HGLOBAL GlobalAlloc(UINT uFlags, SIZE_T dwBytes);
HGLOBAL GlobalFree(HGLOBAL hMem);
SIZE_T GlobalSize(HGLOBAL hMem);
BOOL SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags);
BOOL SetWindowTextW(HWND hWnd, LPCWSTR lpString);
int MessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType);
int GetDateFormatW(LCID locale, DWORD dwFlags, const SYSTEMTIME *lpDate, LPCWSTR lpFormat, LPWSTR lpDateStr, int cchDate);
int GetTimeFormatW(LCID dwLocale, DWORD dwFlags, const SYSTEMTIME *lpTime, LPCWSTR lpFormat, LPWSTR lpTimeStr, int cchTime);
UINT GetDoubleClickTime(void);

#ifdef __cplusplus
}
#endif

#ifndef CBS_DROPDOWN
#define CBS_DROPDOWN 0x0002
#define CBS_DROPDOWNLIST 0x0003
#define CBS_OEMCONVERT 0x0080
#endif

#ifndef ES_OEMCONVERT
#define ES_OEMCONVERT 0x0400
#define ES_AUTOHSCROLL 0x0080
#define ES_READONLY 0x0800
#define ES_PASSWORD 0x0020
#define ES_NUMBER 0x2000
#define ES_MULTILINE 0x0004
#define ES_AUTOVSCROLL 0x0040
#define ES_CENTER 0x0001
#endif

#ifndef SPI_GETWHEELSCROLLLINES
#define SPI_GETWHEELSCROLLLINES 0x0068
#endif
#ifndef WHEEL_DELTA
#define WHEEL_DELTA 120
#endif

#ifndef WM_INPUTLANGCHANGE
#define WM_INPUTLANGCHANGE 0x0051
#define WM_INPUTLANGCHANGEREQUEST 0x0050
#endif

#define ISC_SHOWUIALL 0x0000000F

#ifndef LANG_NEUTRAL
#define LANG_NEUTRAL 0x00
#define SUBLANG_DEFAULT 0x01
#define SORT_DEFAULT 0x0
#define MAKELANGID(p, s) ((DWORD)(((WORD)(s) << 10) | (WORD)(p)))
#define MAKELCID(l, s) ((DWORD)((((DWORD)((WORD)(s))) << 16) | (WORD)(l)))
#define LOCALE_IDEFAULTANSICODEPAGE 0x00001004
#endif

#ifdef __cplusplus
extern "C" {
#endif

HKL GetKeyboardLayout(DWORD thread);
int GetKeyboardLayoutList(int count, HKL *list);
int GetLocaleInfoA(LCID locale, LCTYPE type, LPSTR data, int cch);
#define GetLocaleInfo GetLocaleInfoA

#ifdef __cplusplus
}
#endif

#define IME_CAND_CODE 0x0001

#ifndef PBYTE
typedef BYTE *PBYTE;
#endif

#ifdef __cplusplus
extern "C" {
#endif

BOOL Renegade_GetKeyboardState(PBYTE keyState);
#define GetKeyboardState Renegade_GetKeyboardState

#ifndef FORMAT_MESSAGE_FROM_SYSTEM
#define FORMAT_MESSAGE_FROM_SYSTEM 0x00001000u
#endif
#ifndef FORMAT_MESSAGE_IGNORE_INSERTS
#define FORMAT_MESSAGE_IGNORE_INSERTS 0x00000200u
#endif

DWORD FormatMessageW(DWORD flags, LPCVOID source, DWORD messageId, DWORD languageId,
	LPWSTR buffer, DWORD size, va_list *arguments);
DWORD FormatMessageA(DWORD flags, LPCVOID source, DWORD messageId, DWORD languageId,
	LPSTR buffer, DWORD size, va_list *arguments);
#define FormatMessage FormatMessageA

#ifdef __cplusplus
}
#endif

#endif
