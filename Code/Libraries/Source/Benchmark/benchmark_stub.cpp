#include "Benchmark.h"

int RunBenchmark(int argc, char *argv[], float *floatResult, float *intResult, float *memResult)
{
	(void)argc;
	(void)argv;
	if (floatResult) *floatResult = 1.0f;
	if (intResult) *intResult = 1.0f;
	if (memResult) *memResult = 1.0f;
	return 0;
}
