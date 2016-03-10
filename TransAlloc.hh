#pragma once
#include "config.h"
#include "compiler.hh"
#include "Interface.hh"
#include "Transaction.hh"

class TransAlloc : public Shared {
public:
    static constexpr int alloc_flag = 1;

    // used to free things only if successful commit
    void transFree(void *ptr) {
        Sto::new_item(this, ptr).add_write(0);
    }

    // malloc() which will be freed on abort
    void* transMalloc(size_t sz) {
        void *ptr = malloc(sz);
        Sto::new_item(this, ptr).add_write(alloc_flag);
        return ptr;
    }

    bool lock(TransItem&, Transaction&) { return true; }
    bool check(const TransItem&, const Transaction&) { assert(0); return false; }
    void install(TransItem& item, const Transaction&) {
        if (item.write_value<int>() != alloc_flag) {
            void *ptr = item.key<void*>();
            Transaction::rcu_free(ptr);
        }
    }
    void unlock(TransItem&) {}
    void cleanup(TransItem& item, bool committed) {
        if (!committed && item.write_value<int>() == alloc_flag)
            Transaction::rcu_free(item.key<void*>());
    }
    void print(std::ostream& w, const TransItem& item) const {
        w << "{TransAlloc @" << item.key<void*>();
        if (item.write_value<int>())
            w << ".alloc}";
        else
            w << ".free}";
    }
};
