#pragma once
#include "config.h"
#include "compiler.hh"
#include "Interface.hh"
#include "Transaction.hh"

class TransAlloc : public Shared {
public:
    static constexpr TransItem::flags_type alloc_flag = TransItem::user0_bit;

    // used to free things only if successful commit
    void transFree(void *ptr) {
        Sto::new_item(this, ptr).add_write(0);
    }

    // malloc() which will be freed on abort
    void* transMalloc(size_t sz) {
        void *ptr = malloc(sz);
        Sto::new_item(this, ptr).add_write(0).add_flags(alloc_flag);
        return ptr;
    }

    // delete which only applies if transaction commits
    template <typename T>
    void transDelete(T *x) {
        Sto::new_item(this, x).add_write(&ObjectDestroyer<T>::destroy_and_free);
    }

    // new which will be delete'd on abort.
    // arguments go to T's constructor
    template <typename T, typename... Args>
    T* transNew(Args&&... args) {
        T* ret = new T(std::forward<Args>(args)...);
        Sto::new_item(this, ret).add_write(&ObjectDestroyer<T>::destroy_and_free).add_flags(alloc_flag);
    }

    bool lock(TransItem&, Transaction&) { return true; }
    bool check(const TransItem&, const Transaction&) { assert(0); return false; }
    void install(TransItem& item, const Transaction&) {
        if (item.has_flag(alloc_flag))
            return;
        if (item.write_value<int>() != 0) {
            auto func = item.write_value<void(*)(void*)>();
            Transaction::rcu_call(func, item.key<void*>());
        } else {
            void *ptr = item.key<void*>();
            Transaction::rcu_free(ptr);
        }
    }
    void unlock(TransItem&) {}
    void cleanup(TransItem& item, bool committed) {
        if (committed || !item.has_flag(alloc_flag))
            return;
        if (item.write_value<int>() != 0) {
            auto func = item.write_value<void(*)(void*)>();
            Transaction::rcu_call(func, item.key<void*>());
        } else {
            Transaction::rcu_free(item.key<void*>());
        }
    }
    void print(std::ostream& w, const TransItem& item) const {
        w << "{TransAlloc @" << item.key<void*>();
        if (item.write_value<int>())
            w << ".alloc}";
        else
            w << ".free}";
    }
};
