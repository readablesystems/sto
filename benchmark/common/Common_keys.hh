#include "xxhash.h"
#include "str.hh" // lcdf::Str

#include "DB_structs.hh"

namespace db_common {
namespace keys {

template <typename T>
struct __attribute__((packed)) single_key {
    using mt_key = bench::masstree_key_adapter<single_key<T>>;

    explicit single_key(T id)
        : id(bench::bswap(id)) {}

    bool operator==(const single_key<T>& other) const {
        return id == other.id;
    }
    bool operator!=(const single_key<T>& other) const {
        return !(*this == other);
    }
    operator lcdf::Str() const {
        return lcdf::Str((const char *)this, sizeof(*this));
    }

    T id;

    friend mt_key;

protected:
    single_key() = default;
};

template <typename T, typename U>
struct __attribute__((packed)) double_key {
    using mt_key = bench::masstree_key_adapter<double_key<T, U>>;

    explicit double_key(T id1, U id2)
        : id1(bench::bswap(id1)), id2(bench::bswap(id2)) {}

    bool operator==(const double_key<T, U>& other) const {
        return (id1 == other.id1) && (id2 == other.id2);
    }
    bool operator!=(const double_key<T, U>& other) const {
        return !(*this == other);
    }
    operator lcdf::Str() const {
        return lcdf::Str((const char *)this, sizeof(*this));
    }

    T id1;
    U id2;

    friend mt_key;

protected:
    double_key() = default;
};

};  // namespace keys
};  // namespace db_common

namespace std {

static constexpr size_t xxh_seed = 0xdeadbeefdeadbeef;

template <typename T>
struct hash<db_common::keys::single_key<T>> {
    size_t operator() (const db_common::keys::single_key<T>& key) const {
        return XXH64(&key, sizeof(db_common::keys::single_key<T>), xxh_seed);
    }
};

template <typename T, typename U>
struct hash<db_common::keys::double_key<T, U>> {
    size_t operator() (const db_common::keys::double_key<T, U>& key) const {
        return XXH64(&key, sizeof(db_common::keys::double_key<T, U>), xxh_seed);
    }
};

};  // namespace std
