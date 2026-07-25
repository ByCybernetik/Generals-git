#ifndef RENEGADE_PROCESS_H
#define RENEGADE_PROCESS_H
#include <stdint.h>
#include <pthread.h>
#include "renegade_win32_shim.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned (__stdcall *ThreadFn)(void *);

unsigned long _beginthread(ThreadFn start, unsigned stack, void *arg);
uintptr_t _beginthreadex(void *security, unsigned stack_size, ThreadFn start_address,
	void *arglist, unsigned initflag, unsigned *thrdaddr);

typedef unsigned long (WINAPI *LPTHREAD_START_ROUTINE)(void *);
HANDLE CreateThread(void *lpThreadAttributes, unsigned dwStackSize,
	LPTHREAD_START_ROUTINE lpStartAddress, void *lpParameter,
	unsigned dwCreationFlags, unsigned long *lpThreadId);

#ifndef _P_NOWAIT
#define _P_NOWAIT 1
#endif
#ifndef _P_WAIT
#define _P_WAIT 0
#endif

intptr_t _spawnl(int mode, const char *pathname, const char *arg0, ...);

#ifdef __cplusplus
}
#endif

#endif
