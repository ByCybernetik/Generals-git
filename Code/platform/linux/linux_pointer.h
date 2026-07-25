/*
 * 64-bit pointer helpers for the Generals Linux port.
 * Never store native pointers in Int / unsigned int / DWORD — use uintptr_t.
 */
#ifndef LINUX_POINTER_H
#define LINUX_POINTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline uintptr_t LinuxPointerToUintptr(const void *p)
{
	return (uintptr_t)p;
}

static inline void *LinuxUintptrToPointer(uintptr_t u)
{
	return (void *)u;
}

/* Legacy Win32 APIs that take/return 32-bit "pointer" values. */
static inline void *LinuxULongToPtr(unsigned long v)
{
	return (void *)(uintptr_t)v;
}

static inline unsigned long LinuxPtrToULong(const void *p)
{
	return (unsigned long)(uintptr_t)p;
}

#ifdef __cplusplus
}
#endif

#define POINTER_TO_UINTPTR(p) LinuxPointerToUintptr(p)
#define UINTPTR_TO_POINTER(u) LinuxUintptrToPointer(u)
#define ULONG_TO_PTR(v) LinuxULongToPtr(v)
#define PTR_TO_ULONG(p) LinuxPtrToULong(p)

#endif /* LINUX_POINTER_H */
