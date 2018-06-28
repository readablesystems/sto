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

class TART : public TObject {
public:
    typedef std::string TKey;
    typedef uintptr_t TVal;

    struct Element {
        TKey key;
        TVal val;
        TVersion vers;
        bool poisoned;
    };

    static void loadKey(TID tid, Key &key) {
        Element* e = (Element*) tid;
        key.set(e->key.c_str(), e->key.size());
    }

    typedef typename std::conditional<true, TVersion, TNonopaqueVersion>::type Version_type;
    typedef typename std::conditional<true, TWrapped<TVal>, TNonopaqueWrapped<TVal>>::type wrapped_type;

    static constexpr TransItem::flags_type deleted_bit = TransItem::user0_bit;
    static constexpr TransItem::flags_type absent_bit = TransItem::user0_bit<<1;

    TART() {
        root_.access().setLoadKey(TART::loadKey);
    }

    TVal transGet(TKey k) {
        Key key;
        key.set(k.c_str(), k.size());
        Element* e = (Element*) root_.access().lookup(key);
        auto item = Sto::item(this, e);
        if (e != nullptr && item.has_write()) {
            return item.template write_value<TVal>();
        }
        if (e) {
            if (e->poisoned) {
                throw Transaction::Abort();
            }
            e->vers.observe_read(item);
            return e->val;
        } else {
            absent_vers_.observe_read(item);
            item.add_flags(absent_bit);
            return 0;
        }
    }

    TVal lookup(TKey k) {
        return transGet(k);
    }

    void transPut(TKey k, TVal v) {
        Key art_key;
        art_key.set(k.c_str(), k.size());
        Element* e = (Element*) root_.access().lookup(art_key);
        if (e) {
            if (e->poisoned) {
                throw Transaction::Abort();
            }

            Sto::item(this, e).add_write(v);
            return;
        }

        e = new Element();
        e->key = k;
        e->val = v;
        e->poisoned = true;
        root_.access().insert(art_key, (TID) e, nullptr);
        Sto::item(this, e).add_write(v);
    }

    void insert(TKey k, TVal v) {
        transPut(k, v);
    }

    void erase(TKey k) {
        transPut(k, 0);
    }

    bool lock(TransItem& item, Transaction& txn) override {
        Element* e = item.template key<Element*>();
        if (e == nullptr) {
            return absent_vers_.is_locked_here() || txn.try_lock(item, absent_vers_);
        } else {
            return txn.try_lock(item, e->vers);
        }
    }
    bool check(TransItem& item, Transaction& txn) override {
        Element* e = item.template key<Element*>();
        if (item.has_flag(absent_bit)) {
            // written items are not checked
            // if an item was read w.o absent bit and is no longer found, abort
            return absent_vers_.cp_check_version(txn, item);

        }
        // if an item w/ absent bit and is found, abort
        return e->vers.cp_check_version(txn, item);
    }
    void install(TransItem& item, Transaction& txn) override {
        Element* e = item.template key<Element*>();

        assert(e);
        Key art_key;
        art_key.set(e->key.c_str(), e->key.size());
        e->poisoned = false;
        e->val = item.template write_value<TVal>();
        txn.set_version_unlock(e->vers, item);
    }
    void unlock(TransItem& item) override {
        Element* e = item.template key<Element*>();
        if (e != nullptr) {
            e->vers.cp_unlock(item);
        } else if (absent_vers_.is_locked_here()) {
            Sto::transaction()->set_version(absent_vers_);
            absent_vers_.cp_unlock(item);
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
    Version_type absent_vers_;
    TOpaqueWrapped<ART_OLC::Tree> root_;
};
