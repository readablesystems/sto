#pragma once
#include "config.h"
#include "compiler.hh"
#include <vector>
#include "Interface.hh"
#include "Transaction.hh"
#include "TWrapped.hh"
#include "simple_str.hh"
#include "print_value.hh"
#include "../ART/Tree.h"

static const unsigned char* c_str(std::string s) {
    return (const unsigned char*) s.c_str();
}

class TART : public TObject {
public:
    typedef std::string TKey;
    typedef std::string key_type;
    typedef uintptr_t Value;
    typedef uintptr_t Value_type;

    struct Element {
        TKey key;
        Value val;
        TVersion vers;
    };

    static void loadKey(TID tid, Key &key) {
        // Store the key of the tuple into the key vector
        // Implementation is database specific
        Element* e = (Element*) tid;
        key.set(e->key.c_str(), e->key.size());
    }

    static Value loadValue(TID tid) {
        Element* e = (Element*) tid;
        return e->val;
    }

    static TVersion loadVers(TID tid) {
        Element* e = (Element*) tid;
        return e->vers;
    }


    typedef typename std::conditional<true, TVersion, TNonopaqueVersion>::type Version_type;
    typedef typename std::conditional<true, TWrapped<Value>, TNonopaqueWrapped<Value>>::type wrapped_type;

    static constexpr TransItem::flags_type deleted_bit = TransItem::user0_bit;

    TART() {
        root_.access().setLoadKey(TART::loadKey);
    }

    std::pair<bool, Value> transGet(TKey k) {
        printf("get\n");
        auto item = Sto::item(this, k);
        if (item.has_write()) {
            auto val = item.template write_value<Value>();
            return {true, val};
        } else {
            std::pair<bool, Value> ret;
            Key key;
            key.set(k.c_str(), k.size());
            TID t = root_.access().lookup(key);
            auto val = loadValue(t);
            if (t != 0) {
                loadVers(t).observe_read(item);
            } else {
                vers_.observe_read(item);
            }
            ret = {true, val};
            return ret;
        }
    }

    Value lookup(TKey k) {
        auto result = transGet(k);
        if (!result.first) {
            throw Transaction::Abort();
        } else {
            return result.second;
        }
    }

    void transPut(TKey k, Value v) {
        printf("put\n");
        auto item = Sto::item(this, k);
        item.add_write(v);
        item.clear_flags(deleted_bit);
    }

    void insert(TKey k, Value v) {
        transPut(k, v);
    }

    void erase(TKey k) {
        auto item = Sto::item(this, k);
        item.add_flags(deleted_bit);
        item.add_write(0);
    }

    bool lock(TransItem& item, Transaction& txn) override {
        return vers_.is_locked_here() || txn.try_lock(item, vers_);
    }
    bool check(TransItem& item, Transaction& txn) override {
        TKey key = item.template key<TKey>();
        Key kStr;
        kStr.set(key.c_str(), key.size());
        TID t = root_.access().lookup(kStr);
        // art_leaf* s = art_search(&root_.access(), c_str(key), key.length());
        if (t == 0) {
            return vers_.cp_check_version(txn, item);
        }
        return loadVers(t).cp_check_version(txn, item);
    }
    void install(TransItem& item, Transaction& txn) override {
        Value val = item.template write_value<Value>();
        TKey key = item.template key<TKey>();
        printf("Install\n");

        // if (item.has_flag(deleted_bit)) {
        //     art_delete(&root_.access(), c_str(key), key.length());
        //     txn.set_version(vers_);
        // } else {
        Element* e = new Element();
        e->key = key;
        e->val = val;
        Key kStr;
        kStr.set(key.c_str(), key.size());
        auto ret = root_.access().lookup(kStr);
        bool new_insert;
        printf("a\n");
        printf("%s\n", key.c_str());
        root_.access().insert(kStr, (TID) e, &new_insert);
        printf("b\n");
        // art_leaf* s = art_insert(&root_.access(), c_str(key), key.length(), (void*) val, &new_insert);
        if (new_insert) {
            // vers_.lock_exclusive();
            txn.set_version(vers_);
            // vers_.unlock_exclusive();
        }
        // inserted_node->vers.lock_exclusive();
        // printf("2 %p\n", inserted_node->vers);
        // txn.set_version(inserted_node->vers);
        // inserted_node->vers.unlock_exclusive();
        // }
    }
    void unlock(TransItem& item) override {
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
    TOpaqueWrapped<ART_OLC::Tree> root_;
};
