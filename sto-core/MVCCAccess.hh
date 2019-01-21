#pragma once

#include "MVCCTypes.hh"
#include "Transaction.hh"

// Proxy for TransItem access
class MvAccess {
public:
    template <typename T>
    static void read(TransProxy item, MvHistory<T> *h) {
        Transaction &t = item.transaction();
        TransItem &it = item.item();
        it.__or_flags(TransItem::read_bit);
        it.rdata_.v = Packer<MvHistory<T>*>::pack(t.buf_, h);
    }
};
