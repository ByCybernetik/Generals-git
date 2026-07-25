#ifndef GSI_PLATFORM_THREAD_H
#define GSI_PLATFORM_THREAD_H

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef pthread_mutex_t GSICriticalSection;

typedef struct GSIMutex
{
	pthread_mutex_t mLock;
} GSIMutex;

typedef struct GSIThread
{
	pthread_t thread;
	pthread_attr_t attr;
} GSIThread;

void gsiInitializeCriticalSection(GSICriticalSection *cs);
void gsiDeleteCriticalSection(GSICriticalSection *cs);
void gsiEnterCriticalSection(GSICriticalSection *cs);
void gsiLeaveCriticalSection(GSICriticalSection *cs);

#ifdef __cplusplus
}
#endif

#endif
