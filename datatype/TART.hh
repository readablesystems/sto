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
    typedef uint8_t* Key;
    typedef uint8_t* key_type;
    typedef uintptr_t Value;
    typedef uintptr_t Value_type;

    typedef typename TOpaqueWrapped<Node>::version_type version_type;

    std::pair<bool, Value> transGet(Key k) {
        auto item = Sto::item(this, k);
        if (item.has_write()) {
            return {true, item.template write_value<Value>()};
        } else {
            std::pair<bool, Value> ret;
            lock(vers_);
            auto search = root_.read(item, vers_);
            Value val = lookup(search.second, k, 4, 4);
            ret = {search.first, val};
            unlock(vers_);
            return ret;
        }
    }

    void transPut(Key k, Value v) {
        Sto::item(this, k).acquire_write(vers_, v);
    }

    bool lock(TransItem& item, Transaction& txn) override {
        return txn.try_lock(item, vers_);
    }
    bool check(TransItem& item, Transaction& txn) override {
        return vers_.cp_check_version(txn, item);
    }
    void install(TransItem& item, Transaction& txn) override {
        root_.write(std::move(item.template write_value<Value>()));
        txn.set_version_unlock(vers_, item);
    }
    void unlock(TransItem& item) override {
        vers_.cp_unlock(item);
    }
    void print(std::ostream& w, const TransItem& item) const override {
        w << "{TART<" << typeid(int).name() << "> " << (void*) this;
        if (item.has_read())
            w << " R" << item.read_value<version_type>();
        if (item.has_write())
            w << " =" << item.write_value<int>();
        w << "}";
    }
protected:
    version_type vers_;
    TOpaqueWrapped<Node> root_;
};
