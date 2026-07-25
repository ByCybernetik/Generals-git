/*
**	Command & Conquer Generals(tm) — win.h with Linux platform shims (from Renegade port).
*/

#if _MSC_VER >= 1000
#pragma once
#endif

#ifndef WIN_H
#define WIN_H

#if (_MSC_VER >= 1200)
#pragma warning(push, 3)
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#if defined(GENERALS_LINUX) || defined(RENEGADE_LINUX)
#include <stdint.h>
#ifndef __cdecl
#define __cdecl
#endif
#ifndef __stdcall
#define __stdcall
#endif
#include <windows.h>
#include "io.h"
#include "winres.h"
#include "dlgs.h"
#include "winuser.h"
#include "wingdi.h"
#include "wincon.h"
#include "win32_minimal.h"
#include "renegade_win32_shim.h"
#include "shellapi.h"
#include "mmsystem.h"
#include "atl_stub.h"
#include "ww_wcstring.h"
#include "osdep.h"
#else
#include	<windows.h>
#endif

#ifdef X
#undef X
#endif
#ifdef Y
#undef Y
#endif

#if (_MSC_VER >= 1200)
#pragma warning(pop)
#endif

#if defined(_WINDOWS) || defined(GENERALS_LINUX) || defined(RENEGADE_LINUX)
extern HINSTANCE	ProgramInstance;
extern HWND			MainWindow;
extern bool GameInFocus;

#ifdef _DEBUG
void __cdecl Print_Win32Error(unsigned long win32Error);
#else
#define Print_Win32Error
#endif

#endif

#endif
