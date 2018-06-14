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

    TART() {
        art_tree_init(&root_.access());
    }

    std::pair<bool, Value> transGet(Key k) {
        auto item = Sto::item(this, k);
        if (item.has_write()) {
            auto val = item.template write_value<Value>();
            return {true, val};
        } else {
            std::pair<bool, Value> ret;
            vers_.lock_exclusive();
            auto search = root_.read(item, vers_);
            // Node* n = lookup(search.second, k, 4, 4);
            // Value val = getLeafValue(n);
            uintptr_t val = (uintptr_t) art_search(&search.second, (const unsigned char*) k.c_str(), k.length());
            ret = {search.first, val};
            printf("get key: [%d, %d, %d, %d] val: %lu\n", k[0], k[1], k[2], k[3], val);
            vers_.unlock_exclusive();
            return ret;
        }
    }

    void transPut(Key k, Value v) {
        printf("put key: [%d, %d, %d, %d] val: %lu\n", k[0], k[1], k[2], k[3], v);
        // uint8_t* newKey = (uint8_t*) malloc(sizeof(uint8_t)*4);
        // memcpy(newKey, k, sizeof(uint8_t)*4);
        Sto::item(this, k).acquire_write(vers_, v);
    }

    bool lock(TransItem& item, Transaction& txn) override {
        // call is locked here
        if (vers_.is_locked_here()) {
            return true;
        }
        return txn.try_lock(item, vers_);
    }
    bool check(TransItem& item, Transaction& txn) override {
        return vers_.cp_check_version(txn, item);
    }
    void install(TransItem& item, Transaction& txn) override {
        // root_.write(std::move(item.template write_value<Value>()));
        // only valid on last install
        Value val = item.template write_value<Value>();
        Key key = item.template key<Key>();
        art_insert(&root_.access(), (const unsigned char*) key.c_str(), key.length(), (void*) val);
        // insert(root_.access(), &root_.access(), key, val, 4, 4);
        // printf("install key: [%d, %d, %d, %d] val: %lu\n", key[0], key[1], key[2], key[3], val);
        txn.set_version_unlock(vers_, item);
    }
    void unlock(TransItem& item) override {
        vers_.cp_unlock(item);
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
