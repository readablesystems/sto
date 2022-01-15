#pragma once

#include "MVCCTypes.hh"
#include "Transaction.hh"

// Proxy for TransItem access
class MvAccess {
public:
    template <typename T, size_t Cells>
    static void read(TransProxy item, MvHistory<T, Cells> *h) {
        Transaction &t = item.transaction();
        TransItem &it = item.item();
        it.__or_flags(TransItem::read_bit);
        it.rdata_.v = Packer<MvHistory<T, Cells>*>::pack(t.buf_, h);
    }
};
