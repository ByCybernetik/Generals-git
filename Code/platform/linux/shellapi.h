#ifndef RENEGADE_SHELLAPI_H
#define RENEGADE_SHELLAPI_H

#include <windows.h>

#define SW_HIDE 0
#define SW_SHOWNORMAL 1
#define SW_SHOW 5

#ifdef __cplusplus
extern "C" {
#endif

HINSTANCE ShellExecuteA(HWND hwnd, LPCSTR operation, LPCSTR file, LPCSTR params,
	LPCSTR directory, INT showCmd);
#define ShellExecute ShellExecuteA
HINSTANCE FindExecutableA(LPCSTR lpFile, LPCSTR lpDirectory, LPSTR lpResult);
#define FindExecutable FindExecutableA

#ifndef CSIDL_PERSONAL
#define CSIDL_PERSONAL 0x0005
#endif
#ifndef CSIDL_DESKTOPDIRECTORY
#define CSIDL_DESKTOPDIRECTORY 0x0010
#endif

BOOL SHGetSpecialFolderPathA(HWND hwnd, LPSTR pszPath, int csidl, BOOL fCreate);
#define SHGetSpecialFolderPath SHGetSpecialFolderPathA

#ifdef __cplusplus
}
#endif

#endif
