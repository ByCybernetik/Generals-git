#ifndef RENEGADE_HASH_MAP_COMPAT_H
#define RENEGADE_HASH_MAP_COMPAT_H

#include <unordered_map>
#include <unordered_set>

namespace std {
template<typename Key, typename T, typename Hash, typename Pred>
using hash_map = std::unordered_map<Key, T, Hash, Pred>;

template<typename Key, typename Hash, typename Pred>
using hash_set = std::unordered_set<Key, Hash, Pred>;
}

#endif
