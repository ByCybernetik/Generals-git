/*
** Linux adaptation layer for GameMemory (DMA + pool blob backing).
**
** DynamicMemoryAllocator uses libc heap blocks with a prefix cookie instead of
** sub-pool free lists. MemoryPool blobs use one heap allocation per slot
** instead of a single contiguous blob (avoids glibc mmap / stack overlap).
*/

#pragma once

#include <cstdint>

struct PoolInitRec;

class LinuxDmaAllocator
{
public:
	LinuxDmaAllocator();
	~LinuxDmaAllocator();

	void init(int numClasses, const PoolInitRec *parms);
	void shutdown();
	void reset();

	void *allocate(int numBytes);
	void free(void *userPtr);
	bool owns(const void *userPtr) const;
	int actualAllocationSize(int numBytes) const;

private:
	static constexpr std::uint32_t kMagic = 0x47444D41u; /* 'GDMA' */

	int m_numClasses;
	int m_sizeClasses[8];
	int m_usedBlocks;
	void *m_liveHead;
};

bool linuxPointerIsPlausibleHeap(const void *p);

void *linuxPoolMallocRawBlock(size_t rawBlockSize);
void linuxPoolFreeRawBlock(void *rawBlock);

void linuxInitGameMemoryHeapPolicy(void);
