#pragma once
#include "config.h"
#include "compiler.hh"
#include <vector>
#include "Interface.hh"
#include "Transaction.hh"
#include "TWrapped.hh"
#include "simple_str.hh"
#include "print_value.hh"
#include "ART.hh"

class TART : public TObject {
public:
    typedef std::string Key;
    typedef std::string key_type;
    typedef uintptr_t Value;
    typedef uintptr_t Value_type;

    typedef typename std::conditional<true, TVersion, TNonopaqueVersion>::type Version_type;
    typedef typename std::conditional<true, TWrapped<Value>, TNonopaqueWrapped<Value>>::type wrapped_type;

    static constexpr TransItem::flags_type deleted_bit = TransItem::user0_bit;

    TART() {
        art_tree_init(&root_.access());
    }

    std::pair<bool, Value> transGet(Key k) {
        auto item = Sto::item(this, k);
        // if (item.has_flag(deleted_bit)) {
        //     return {true, NULL};
        // }

        if (item.has_write()) {
            auto val = item.template write_value<Value>();
            return {true, val};
        } else {
            std::pair<bool, Value> ret;
            vers_.lock_exclusive();
            art_leaf* leaf = art_search(&root_.access(), (const unsigned char*) k.c_str(), k.length());
            Value val = 0;
            if (leaf != NULL) {
                val = (Value) leaf->value;
                leaf->vers.observe_read(item);
            } else {
                vers_.observe_read(item);
            }
            vers_.unlock_exclusive();
            ret = {true, val};
            printf("get key: %s, val: %lu\n", k.c_str(), val);
            return ret;
        }
    }

    Value lookup(Key k) {
        auto result = transGet(k);
        if (!result.first) {
            throw Transaction::Abort();
        } else {
            return result.second;
        }
    }

    void transPut(Key k, Value v) {
        printf("put key: %s, val: %lu\n", k.c_str(), v);
        auto item = Sto::item(this, k);
        item.add_write(v);
        item.clear_flags(deleted_bit);
    }

    void insert(Key k, Value v) {
        transPut(k, v);
    }

    void erase(Key k) {
        printf("erase key: %s\n", k.c_str());
        auto item = Sto::item(this, k);
        item.add_flags(deleted_bit);
        item.add_write(0);
    }

    bool lock(TransItem& item, Transaction& txn) override {
        if (vers_.is_locked_here()) {
            return true;
        }
        printf("LOCK\n");
        printf("%p\n", vers_);
        return txn.try_lock(item, vers_);
    }
    bool check(TransItem& item, Transaction& txn) override {
        printf("CHECK\n");
        Key key = item.template key<Key>();
        art_leaf* s = art_search(&root_.access(), (const unsigned char*) key.c_str(), key.length());
        if (s == NULL) {
            return vers_.cp_check_version(txn, item);
        }
        return s->vers.cp_check_version(txn, item);
    }
    void install(TransItem& item, Transaction& txn) override {
        printf("INSTALL\n");
        Value val = item.template write_value<Value>();
        Key key = item.template key<Key>();

        if (item.has_flag(deleted_bit)) {
            art_delete(&root_.access(), (const unsigned char*) key.c_str(), key.length());
            txn.set_version(vers_);
        } else {
            auto new_insert = art_insert(&root_.access(), (const unsigned char*) key.c_str(), key.length(), (void*) val);
            if (new_insert == NULL) {
                txn.set_version(vers_);
            }
            art_leaf* s = art_search(&root_.access(), (const unsigned char*) key.c_str(), key.length());
            s->vers.lock_exclusive();
            txn.set_version(s->vers);
            s->vers.unlock_exclusive();
        }
        printf("d\n");

        printf("install key: %s, val: %lu\n", key.c_str(), val);
    }
    void unlock(TransItem& item) override {
        printf("UNLOCK\n");
        if (vers_.is_locked_here()) {
            vers_.cp_unlock(item);
        }
    }
    void print(std::ostream& w, const TransItem& item) const override {
        w << "{TART<" << typeid(int).name() << "> " << (void*) this;
        if (item.has_read())
            w << " R" << item.read_value<Version_type>();
        if (item.has_write())
            w << " =" << item.write_value<int>();
        w << "}";
    }
protected:
    Version_type vers_;
    TOpaqueWrapped<art_tree> root_;
};
