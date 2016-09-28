#pragma once
#include "config.h"
#include "compiler.hh"
#include "Interface.hh"
#include "Transaction.hh"

class TransAlloc : public TObject {
public:
    static constexpr TransItem::flags_type alloc_flag = TransItem::user0_bit;
    typedef void (*free_type)(void*);

    // used to free things only if successful commit
    void transFree(void *ptr) {
        Sto::new_item(this, ptr).template add_write<free_type, free_type>(::free);
    }

    // malloc() which will be freed on abort
    void* transMalloc(size_t sz) {
        void *ptr = malloc(sz);
        Sto::new_item(this, ptr).template add_write<free_type, free_type>(::free).add_flags(alloc_flag);
        return ptr;
    }

    // delete which only applies if transaction commits
    template <typename T>
    void transDelete(T *x) {
        Sto::new_item(this, x).template add_write<free_type, free_type>(&ObjectDestroyer<T>::destroy_and_free);
    }

    // new which will be delete'd on abort.
    // arguments go to T's constructor
    template <typename T, typename... Args>
    T* transNew(Args&&... args) {
        T* x = new T(std::forward<Args>(args)...);
        Sto::new_item(this, x).template add_write<free_type, free_type>(&ObjectDestroyer<T>::destroy_and_free).add_flags(alloc_flag);
        return x;
    }

    bool lock(TransItem&, Transaction&) override { return true; }
    bool check(TransItem&, Transaction&) override { return false; }
    void install(TransItem&, Transaction&) override {}
    void unlock(TransItem&) override {}
    void cleanup(TransItem& item, bool committed) override {
        if (committed == !item.has_flag(alloc_flag))
	  Transaction::rcu_call(item.write_value<free_type>(), item.key<void*>());
    }
    void print(std::ostream& w, const TransItem& item) const override {
        w << "{TransAlloc @" << item.key<void*>();
        if (item.write_value<int>())
            w << ".alloc}";
        else
            w << ".free}";
    }
};
