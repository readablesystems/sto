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
        Element* next;
    };

    static void loadKey(TID tid, Key &key) {
        Element* e = (Element*) tid;
        key.set(e->key.c_str(), e->key.size());
    }

    static TVal loadValue(TID tid) {
        Element* e = (Element*) tid;
        return e->val;
    }

    static TVersion loadVers(TID tid) {
        Element* e = (Element*) tid;
        return e->vers;
    }

    typedef typename std::conditional<true, TVersion, TNonopaqueVersion>::type Version_type;
    typedef typename std::conditional<true, TWrapped<TVal>, TNonopaqueWrapped<TVal>>::type wrapped_type;

    static constexpr TransItem::flags_type deleted_bit = TransItem::user0_bit;
    static constexpr TransItem::flags_type absent_bit = TransItem::user0_bit<<1;

    TART() {
        root_.access().setLoadKey(TART::loadKey);
    }

    TVal transGet(TKey k) {
        auto headItem = Sto::item(this, -1);
        Element* next = nullptr;
        if (headItem.has_write()) {
            next = headItem.template write_value<Element*>();
        }
        bool found;
        while (next) {
            if (next->key.compare(k) == 0) {
                return next->val;
            }
            next = next->next;
        }
        Key key;
        key.set(k.c_str(), k.size());
        Element* e = (Element*) root_.access().lookup(key);
        auto item = Sto::item(this, e);
        if (e) {
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
        auto headItem = Sto::item(this, -1);
        Element* next = nullptr;
        if (headItem.has_write()) {
            next = headItem.template write_value<Element*>();
        }
        bool found;
        while (next) {
            if (next->key.compare(k) == 0) {
                auto item = Sto::item(this, next);
                item.add_write(v);
                item.clear_flags(deleted_bit);
                next->val = v;
                return;
            }
            next = next->next;
        }
        Element* e = new Element();
        e->key = k;
        e->val = v;
        auto item = Sto::item(this, e);

        if (headItem.has_write()) {
            e->next = headItem.template write_value<Element*>();
        } else {
            e->next = nullptr;
        }
        headItem.add_write(e);
        item.add_write(v);
        item.clear_flags(deleted_bit);
    }

    void insert(TKey k, TVal v) {
        transPut(k, v);
    }

    void erase(TKey k) {
        auto headItem = Sto::item(this, -1);
        Element* next = nullptr;
        if (headItem.has_write()) {
            next = headItem.template write_value<Element*>();
        }
        bool found;
        while (next) {
            if (next->key.compare(k) == 0) {
                auto item = Sto::item(this, next);
                item.add_write(0);
                item.add_flags(deleted_bit);
                next->val = 0;
                return;
            }
            next = next->next;
        }
        Element* e = new Element();
        e->key = k;
        e->val = 0;
        auto item = Sto::item(this, e);

        if (headItem.has_write()) {
            e->next = headItem.template write_value<Element*>();
        } else {
            e->next = nullptr;
        }
        headItem.add_write(e);
        item.add_write(0);
        item.add_flags(deleted_bit);
    }

    bool lock(TransItem& item, Transaction& txn) override {
        Element* e = item.template key<Element*>();
        if ((long) e == 0xffffffff) { return true; }
        if (e == nullptr) {
            return absent_vers_.is_locked_here() || txn.try_lock(item, absent_vers_);
        } else {
            return txn.try_lock(item, e->vers);
        }
    }
    bool check(TransItem& item, Transaction& txn) override {
        Element* e = item.template key<Element*>();
        if ((long) e == 0xffffffff) { return true; }
        if (e == nullptr) {
            // written items are not checked
            // if an item was read w.o absent bit and is no longer found, abort
            return item.has_flag(absent_bit) && absent_vers_.cp_check_version(txn, item);

        }
        // if an item w/ absent bit and is found, abort
        return !item.has_flag(absent_bit) && e->vers.cp_check_version(txn, item);
    }
    void install(TransItem& item, Transaction& txn) override {
        Element* e = item.template key<Element*>();

        // if (item.has_flag(deleted_bit)) {
        //     art_delete(&root_.access(), c_str(key), key.length());
        //     txn.set_version(vers_);
        // } else {
        if ((long) e == 0xffffffff) { return; }
        if (e) {
            Key art_key;
            art_key.set(e->key.c_str(), e->key.size());
            Element* ret = (Element*) root_.access().lookup(art_key);
            if (ret == 0) {
                if (!Sto::item(this, -1).has_flag(deleted_bit)) {
                    if (!absent_vers_.is_locked_here()) absent_vers_.lock_exclusive();
                    txn.set_version(absent_vers_);
                    Sto::item(this, -1).add_flags(deleted_bit);
                    absent_vers_.unlock_exclusive();
                }
                root_.access().insert(art_key, (TID) e, nullptr);
            } else {
                // update
                ret->val = e->val;
                txn.set_version_unlock(ret->vers, item);
            }
        }
    }
    void unlock(TransItem& item) override {
        Element* e = item.template key<Element*>();
        if ((long) e == 0xffffffff) { return; }
        if (e != 0) {
            e->vers.cp_unlock(item);
        }
        Sto::item(this, -1).clear_flags(deleted_bit);
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
