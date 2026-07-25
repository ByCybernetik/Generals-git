/*
** Linux adaptation layer for GameMemory.
*/

#include "linux_game_memory.h"

#include "Common/GameMemory.h"
#include "Common/Errors.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#ifdef __GLIBC__
#include <malloc.h>
#endif

namespace {

struct LinuxDmaBlockHeader
{
	std::uint32_t magic;
	std::uint32_t userSize;
	std::uint32_t slotSize;
	std::uint32_t pad;
	LinuxDmaBlockHeader *nextLive;
	LinuxDmaBlockHeader *prevLive;
};

static_assert(sizeof(LinuxDmaBlockHeader) == 32, "LinuxDmaBlockHeader layout");

LinuxDmaBlockHeader *headerFromUser(void *userPtr)
{
	return reinterpret_cast<LinuxDmaBlockHeader *>(static_cast<char *>(userPtr) - sizeof(LinuxDmaBlockHeader));
}

const LinuxDmaBlockHeader *headerFromUser(const void *userPtr)
{
	return reinterpret_cast<const LinuxDmaBlockHeader *>(static_cast<const char *>(userPtr) - sizeof(LinuxDmaBlockHeader));
}

} // namespace

//-----------------------------------------------------------------------------
bool linuxPointerIsPlausibleHeap(const void *p)
{
	if (p == NULL) {
		return false;
	}
	const uintptr_t addr = reinterpret_cast<uintptr_t>(p);
	if (addr <= 0xffffULL) {
		return false;
	}
	/* Truncated 64-bit pointers: upper bits set, lower 32 bits zeroed. */
	if ((addr & 0xffffffffULL) == 0ULL) {
		return false;
	}
	const uintptr_t sp = reinterpret_cast<uintptr_t>(__builtin_frame_address(0));
	if (addr <= sp && (sp - addr) < 0x2000000ULL) {
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
void linuxInitGameMemoryHeapPolicy(void)
{
#ifdef __GLIBC__
	mallopt(M_MMAP_MAX, 0);
#endif
}

//-----------------------------------------------------------------------------
void *linuxPoolMallocRawBlock(size_t rawBlockSize)
{
	if (rawBlockSize == 0) {
		return NULL;
	}
	void *p = std::malloc(rawBlockSize);
	if (p == NULL) {
		throw ERROR_OUT_OF_MEMORY;
	}
	if (!linuxPointerIsPlausibleHeap(p)) {
		std::free(p);
		throw ERROR_OUT_OF_MEMORY;
	}
	return p;
}

//-----------------------------------------------------------------------------
void linuxPoolFreeRawBlock(void *rawBlock)
{
	std::free(rawBlock);
}

//-----------------------------------------------------------------------------
LinuxDmaAllocator::LinuxDmaAllocator() :
	m_numClasses(0),
	m_usedBlocks(0),
	m_liveHead(NULL)
{
	std::memset(m_sizeClasses, 0, sizeof(m_sizeClasses));
}

//-----------------------------------------------------------------------------
LinuxDmaAllocator::~LinuxDmaAllocator()
{
	shutdown();
}

//-----------------------------------------------------------------------------
void LinuxDmaAllocator::init(int numClasses, const PoolInitRec *parms)
{
	shutdown();
	if (numClasses <= 0 || parms == NULL) {
		return;
	}
	if (numClasses > 8) {
		numClasses = 8;
	}
	m_numClasses = numClasses;
	for (int i = 0; i < m_numClasses; ++i) {
		m_sizeClasses[i] = parms[i].allocationSize;
	}
}

//-----------------------------------------------------------------------------
void LinuxDmaAllocator::shutdown()
{
	reset();
	m_numClasses = 0;
	std::memset(m_sizeClasses, 0, sizeof(m_sizeClasses));
}

//-----------------------------------------------------------------------------
void LinuxDmaAllocator::reset()
{
	LinuxDmaBlockHeader *head = static_cast<LinuxDmaBlockHeader *>(m_liveHead);
	while (head != NULL) {
		LinuxDmaBlockHeader *h = head;
		head = h->nextLive;
		h->magic = 0;
		h->nextLive = NULL;
		h->prevLive = NULL;
		std::free(h);
	}
	m_liveHead = NULL;
	m_usedBlocks = 0;
}

//-----------------------------------------------------------------------------
int LinuxDmaAllocator::actualAllocationSize(int numBytes) const
{
	for (int i = 0; i < m_numClasses; ++i) {
		if (numBytes <= m_sizeClasses[i]) {
			return m_sizeClasses[i];
		}
	}
	return numBytes;
}

//-----------------------------------------------------------------------------
void *LinuxDmaAllocator::allocate(int numBytes)
{
	if (numBytes < 0) {
		throw ERROR_OUT_OF_MEMORY;
	}
	const int slotSize = actualAllocationSize(numBytes);
	const size_t total = sizeof(LinuxDmaBlockHeader) + static_cast<size_t>(slotSize);
	LinuxDmaBlockHeader *h = static_cast<LinuxDmaBlockHeader *>(std::malloc(total));
	if (h == NULL) {
		throw ERROR_OUT_OF_MEMORY;
	}
	h->magic = kMagic;
	h->userSize = static_cast<std::uint32_t>(numBytes);
	h->slotSize = static_cast<std::uint32_t>(slotSize);
	h->pad = 0;
	LinuxDmaBlockHeader *oldHead = static_cast<LinuxDmaBlockHeader *>(m_liveHead);
	h->nextLive = oldHead;
	h->prevLive = NULL;
	if (oldHead != NULL) {
		oldHead->prevLive = h;
	}
	m_liveHead = h;
	++m_usedBlocks;
	return reinterpret_cast<char *>(h) + sizeof(LinuxDmaBlockHeader);
}

//-----------------------------------------------------------------------------
bool LinuxDmaAllocator::owns(const void *userPtr) const
{
	if (userPtr == NULL) {
		return false;
	}
	const LinuxDmaBlockHeader *h = headerFromUser(userPtr);
	if (!linuxPointerIsPlausibleHeap(h)) {
		return false;
	}
	return h->magic == kMagic;
}

//-----------------------------------------------------------------------------
void LinuxDmaAllocator::free(void *userPtr)
{
	if (userPtr == NULL) {
		return;
	}
	LinuxDmaBlockHeader *h = headerFromUser(userPtr);
	if (h->magic != kMagic) {
		return;
	}
	h->magic = 0;

	/*
	 * O(1) unlink via prev/next. A single-linked walk was O(n) per free and
	 * O(n^2) when clearing large maps (Dict/AsciiString/scripts), which
	 * dominated exit-to-menu time.
	 */
	if (h->prevLive != NULL) {
		h->prevLive->nextLive = h->nextLive;
	} else {
		m_liveHead = h->nextLive;
	}
	if (h->nextLive != NULL) {
		h->nextLive->prevLive = h->prevLive;
	}
	h->nextLive = NULL;
	h->prevLive = NULL;

	std::free(h);
	--m_usedBlocks;
	if (m_usedBlocks < 0) {
		m_usedBlocks = 0;
	}
}
