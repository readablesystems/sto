#pragma once

#include "Transaction.hh"

#define SIZE (1<<15)
class GenericSTM : public Shared {
public:
    GenericSTM()
      : table_() {
    }

    template <typename T>
    T transRead(T* word) {
        static_assert(sizeof(T) <= sizeof(void*), "don't support words larger than pointer size");

        auto it = Sto::item(this, word);
        if (it.has_write()) {
            return it.template write_value<T>();
        }
        size_t key = bucket(word);
        // ensure version doesn't change
        auto version = table_[key];
        Sto::check_opacity(version);
        it.add_read(version);
        fence();
        T ret = *word;
        return ret;
    }

    template <typename T>
    void transWrite(T* word, const T& new_val) {
        static_assert(sizeof(T) <= sizeof(void*), "don't support words larger than pointer size");
        // just makes the version number change, i.e., makes conflicting reads abort
        // (and locks this word for us)
        Sto::item(this, word).add_write(new_val).assign_flags(sizeof(T) << TransItem::userf_shift);
    }

    bool lock(TransItem& item, Transaction&) {
        size_t key = bucket(item.key<void*>());
        if (!TransactionTid::is_locked_here(table_[key]))
            TransactionTid::lock(table_[key]);
        return true;
    }
    bool check(const TransItem& item, const Transaction&) {
        size_t key = bucket(item.key<void*>());
        auto current = table_[key];
        return TransactionTid::check_version(current, item.read_value<uint64_t>());
    }
    void install(TransItem& item, const Transaction& t) {
        void* word = item.key<void*>();
        // need to know when we've unlocked it so we keep threadid in there for now
        TransactionTid::set_version(table_[bucket(word)], t.commit_tid());
        void *data = item.write_value<void*>();
        memcpy(word, &data, item.shifted_user_flags());
    }
    void unlock(TransItem& item) {
        size_t key = bucket(item.key<void*>());
        if (TransactionTid::is_locked_here(table_[key]))
            TransactionTid::unlock(table_[key]);
    }

private:
    inline size_t hash(void* k) {
        // std::hash<void*> hash_fn;
        // return hash_fn(k);
        return ((size_t)k) >> 3;
    }
  
    inline size_t nbuckets() {
        return SIZE;
    }
    inline size_t bucket(void* k) {
        return hash(k) % nbuckets();
    }
    uint64_t table_[SIZE];
};
