#pragma once

#include <string>
#include <list>
#include <cassert>
#include <variant>

#include "DB_structs.hh"
#include "xxhash.h"
#include "str.hh" // lcdf::Str
#include "Adapter.hh"
#include "Accessor.hh"

#ifndef TABLE_FINE_GRAINED
#define TABLE_FINE_GRAINED 0
#endif

namespace dynamic {

using namespace bench;

#include "Dynamic_structs_generated.hh"

struct ordered_key {
    ordered_key(uint64_t id) {
        o_id = bswap(id);
    }
    ordered_key(const lcdf::Str& mt_key) {
        assert(mt_key.length() == sizeof(*this));
        memcpy(this, mt_key.data(), sizeof(*this));
    }
    bool operator==(const ordered_key& other) const {
        return o_id == other.o_id;
    }
    bool operator!=(const ordered_key& other) const {
        return !(*this == other);
    }
    operator lcdf::Str() const {
        return lcdf::Str((const char *)this, sizeof(*this));
    }

    uint64_t o_id;
};

struct unordered_key {
    unordered_key(uint64_t id) {
        u_id = bswap(id);
    }
    bool operator==(const unordered_key& other) const {
        return u_id == other.u_id;
    }
    bool operator!=(const unordered_key& other) const {
        return !(*this == other);
    }
    operator lcdf::Str() const {
        return lcdf::Str((const char *)this, sizeof(*this));
    }

    uint64_t u_id;
};

}; // namespace dynamic

namespace std {

static constexpr size_t xxh_seed = 0xdeadbeefdeadbeef;

using bench::var_string;
using bench::fix_string;
using bench::bswap;

template <size_t ML>
struct hash<var_string<ML>> {
    size_t operator()(const var_string<ML>& arg) const {
        return XXH64(arg.s_, ML + 1, xxh_seed);
    }
};

template <size_t FL>
struct hash<fix_string<FL>> {
    size_t operator()(const fix_string<FL>& arg) const {
        return XXH64(arg.s_, FL, xxh_seed);
    }
};

template <>
struct hash<dynamic::ordered_key> {
    size_t operator()(const dynamic::ordered_key& arg) const {
        return bswap(arg.o_id);
    }
};

inline ostream& operator<<(ostream& os, const dynamic::ordered_key& ok) {
    os << "ordered_key:" << bswap(ok.o_id);
    return os;
}

template <>
struct hash<dynamic::unordered_key> {
    size_t operator()(const dynamic::unordered_key& arg) const {
        return bswap(arg.u_id);
    }
};

inline ostream& operator<<(ostream& os, const dynamic::unordered_key& uk) {
    os << "unordered_key:" << bswap(uk.u_id);
    return os;
}

}; // namespace std
