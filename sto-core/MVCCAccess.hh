#pragma once

#include "Transaction.hh"

// Proxy for TransItem access
class TMvAccess {
private:
    template <typename T>
    static void read(TransProxy item, MvHistory<T> *h) {
        Transaction &t = item.transaction();
        TransItem &it = item.item();
        it.__or_flags(TransItem::read_bit);
        it.rdata_.v = Packer<MvHistory<T>*>::pack(t.buf_, h);
    }

    template <typename T, unsigned N>
    friend class TMvArray;
    template <typename T, unsigned N>
    friend class TMvArrayAdaptive;
    template <typename T>
    friend class TMvBox;
};
