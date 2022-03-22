#pragma once

#include "MVCCTypes.hh"
#include "Transaction.hh"

// Proxy for TransItem access
class MvAccess {
public:
    // Retrieve stored version in read data
    template <typename T, size_t Cells>
    static void get_read(TransProxy item, MvHistory<T, Cells> **h) {
        TransItem &it = item.item();
        *h = Packer<MvHistory<T, Cells>*>::unpack(it.rdata_.v);
    }

    // Set read bit and store version in read data
    template <typename T, size_t Cells>
    static void read(TransProxy item, MvHistory<T, Cells> *h) {
        Transaction &t = item.transaction();
        TransItem &it = item.item();
        it.__or_flags(TransItem::read_bit);
        it.rdata_.v = Packer<MvHistory<T, Cells>*>::pack(t.buf_, h);
    }

    // Set read bit only
    static void set_read(TransProxy item) {
        TransItem &it = item.item();
        it.__or_flags(TransItem::read_bit);
    }

    // Store version in read data without setting the read bit
    template <typename T, size_t Cells>
    static void store_read(TransProxy item, MvHistory<T, Cells> *h) {
        Transaction &t = item.transaction();
        TransItem &it = item.item();
        it.rdata_.v = Packer<MvHistory<T, Cells>*>::pack(t.buf_, h);
    }
};
