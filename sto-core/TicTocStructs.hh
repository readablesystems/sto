#pragma once

// used to store TicToc version number
struct WideTid {
    typedef uint64_t single_type;
    typedef int64_t signed_single_type;

    WideTid() = default;
    WideTid(const WideTid&) = delete;
    WideTid& operator=(const WideTid&) = delete;
    WideTid& operator=(WideTid&&) = delete;

    single_type v0;
    single_type v1;
};

// Union struct should result in only one extra 8-byte word than before
// for each TItem
union rdata_t {
    void *  v;
    WideTid w;
};
