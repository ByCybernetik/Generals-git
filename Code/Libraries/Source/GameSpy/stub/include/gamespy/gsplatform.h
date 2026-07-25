/* Minimal GameSpy platform shim for Generals Linux port (no vendor SDK). */
#ifndef GSI_PLATFORM_H
#define GSI_PLATFORM_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef gsi_char
#define gsi_char char
#endif

typedef int gsi_bool;
#define gsi_true 1
#define gsi_false 0

typedef int32_t gsi_i32;
typedef int64_t gsi_i64;
typedef uint16_t gsi_u16;
typedef unsigned int gsi_time;
typedef int SOCKET;

#ifndef GSI_MAX_INTEGRAL_BITS
#define GSI_MAX_INTEGRAL_BITS 64
#endif

#define msleep(ms) usleep((unsigned int)(ms) * 1000)

#endif
