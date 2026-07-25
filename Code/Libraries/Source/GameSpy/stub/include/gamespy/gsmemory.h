#ifndef GSI_MEMORY_H
#define GSI_MEMORY_H

#include <stdlib.h>

#define gsifree(p) free(p)
#define gsimalloc(sz) malloc(sz)
#define gsirealloc(p, sz) realloc((p), (sz))
#define gsicalloc(n, sz) calloc((n), (sz))

#endif
