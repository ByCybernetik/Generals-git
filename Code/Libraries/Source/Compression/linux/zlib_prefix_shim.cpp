/* Bridge Generals Z_PREFIX zlib names to system zlib. */
#include <zlib.h>

extern "C" int z_compress2(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen, int level)
{
	return compress2(dest, destLen, source, sourceLen, level);
}

extern "C" int z_uncompress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen)
{
	return uncompress(dest, destLen, source, sourceLen);
}
