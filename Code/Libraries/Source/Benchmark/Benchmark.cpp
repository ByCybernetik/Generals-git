#include "Benchmark.h"

/* Lightweight Linux stub: original Westwood CPU/memory microbench is not shipped. */
int RunBenchmark(int argc, char *argv[], float *floatResult, float *intResult, float *memResult)
{
	(void)argc;
	(void)argv;
	if (floatResult) {
		*floatResult = 100.0f;
	}
	if (intResult) {
		*intResult = 100.0f;
	}
	if (memResult) {
		*memResult = 100.0f;
	}
	return 0;
}
