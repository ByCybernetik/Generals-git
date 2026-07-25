/*
 * STL allocator for linux_registry_ini.cpp — uses libc malloc/free so registry
 * parsing does not go through the game's pooled global operator new.
 */
#ifndef LINUX_REGISTRY_ALLOC_H
#define LINUX_REGISTRY_ALLOC_H

#include <cstddef>
#include <cstdlib>
#include <limits>
#include <map>
#include <new>
#include <string>
#include <vector>

template <typename T>
struct LinuxRegMallocAllocator {
	using value_type = T;

	LinuxRegMallocAllocator() noexcept = default;
	template <typename U>
	LinuxRegMallocAllocator(const LinuxRegMallocAllocator<U> &) noexcept {}

	T *allocate(std::size_t n)
	{
		if (n > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
			throw std::bad_alloc();
		}
		void *p = std::malloc(n * sizeof(T));
		if (p == nullptr) {
			throw std::bad_alloc();
		}
		return static_cast<T *>(p);
	}

	void deallocate(T *p, std::size_t) noexcept { std::free(p); }
};

template <typename T, typename U>
bool operator==(const LinuxRegMallocAllocator<T> &, const LinuxRegMallocAllocator<U> &) noexcept
{
	return true;
}

template <typename T, typename U>
bool operator!=(const LinuxRegMallocAllocator<T> &, const LinuxRegMallocAllocator<U> &) noexcept
{
	return false;
}

using RegString = std::basic_string<char, std::char_traits<char>, LinuxRegMallocAllocator<char>>;
using RegBytes = std::vector<unsigned char, LinuxRegMallocAllocator<unsigned char>>;

template <typename Key, typename Val>
using RegMap = std::map<Key, Val, std::less<Key>, LinuxRegMallocAllocator<std::pair<const Key, Val>>>;

template <typename T>
static T &linux_reg_lazy_singleton(void)
{
	static T *ptr = nullptr;
	if (ptr == nullptr) {
		void *mem = std::malloc(sizeof(T));
		if (mem == nullptr) {
			throw std::bad_alloc();
		}
		ptr = new (mem) T();
	}
	return *ptr;
}

#endif /* LINUX_REGISTRY_ALLOC_H */
