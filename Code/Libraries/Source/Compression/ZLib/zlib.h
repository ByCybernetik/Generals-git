#pragma once
/* Linux shim: Generals uses Z_PREFIX names against system zlib. */
#include <zlib.h>

#ifdef __cplusplus
extern "C" {
#endif

int z_compress2(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen, int level);
int z_uncompress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen);

#ifdef __cplusplus
}
#endif
