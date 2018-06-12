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
    typedef uint8_t Key[4];
    typedef uint8_t key_type[4];
    typedef uintptr_t Value;
    typedef uintptr_t Value_type;
    typedef std::pair<Key, Value> item_value;

    typedef typename std::conditional<true, TVersion, TNonopaqueVersion>::type Version_type;
    typedef typename std::conditional<true, TWrapped<Value>, TNonopaqueWrapped<Value>>::type wrapped_type;

    TART() {
        leaves = new uint32_t[0];
    }

    std::pair<bool, Value> transGet(Key k) {
        auto item = Sto::item(this, k);
        if (item.has_write()) {
            auto val = item.template write_value<item_value>();
            return {true, val.second};
        } else {
            std::pair<bool, Value> ret;
            vers_.lock_exclusive();
            auto search = root_.read(item, vers_);
            Node* n = lookup(search.second, k, 4, 4);
            Value val = getLeafValue(n);
            ret = {search.first, val};
            vers_.unlock_exclusive();
            return ret;
        }
    }

    void transPut(Key k, Value v) {
        std::pair<uintptr_t, uintptr_t> wv = {(uintptr_t) k, v};
        Sto::item(this, k).acquire_write(vers_, wv);
    }

    bool lock(TransItem& item, Transaction& txn) override {
        // call is locked here
        return txn.try_lock(item, vers_);
    }
    bool check(TransItem& item, Transaction& txn) override {
        return vers_.cp_check_version(txn, item);
    }
    void install(TransItem& item, Transaction& txn) override {
        // root_.write(std::move(item.template write_value<Value>()));
        // only valid on last install
        item_value val = item.template write_value<item_value>();
        insert(root_.access(), &root_.access(), val.first, val.second, 4, 4);
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
    TOpaqueWrapped<Node*> root_;
};
