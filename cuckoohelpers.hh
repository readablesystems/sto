#pragma once

#define LIBCUCKOO_DEBUG 0
#if LIBCUCKOO_DEBUG
#  define LIBCUCKOO_DBG(fmt, args...)  fprintf(stderr, "\x1b[32m""[libcuckoo:%s:%d:%lu] " fmt"" "\x1b[0m",__FILE__,__LINE__, (unsigned long)pthread_self(), ##args)
#else
#  define LIBCUCKOO_DBG(fmt, args...)  do {} while (0)
#endif

/* hashed_key<key_type> hashes the given key. */
template <class Key>
inline size_t hashed_key(const Key &key) {
    return std::hash<Key>()(key);
}

/* number of keys that can be stored in one bucket */
const size_t SLOT_PER_BUCKET = 20;

// The maximum number of cuckoo operations per insert. This must
// be less than or equal to SLOT_PER_BUCKET^(MAX_BFS_DEPTH+1)
const size_t MAX_CUCKOO_COUNT = 500;

// The maximum depth of a BFS path
const size_t MAX_BFS_DEPTH = 4;
