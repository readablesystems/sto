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
#include "str.hh"

class TART : public TObject {
public:
    typedef std::pair<const char*, size_t> TKey;
    typedef uintptr_t TVal;
    typedef ART_OLC::N Node;

    struct Element {
        TKey key;
        TVal val;
        TVersion vers;
        bool poisoned;
    };

    static void loadKey(TID tid, Key &key) {
        Element* e = (Element*) tid;
        auto ek = e->key;
        auto len = ek.second;
        key.setKeyLen(len);
        if (len > 8) {
            key.set(ek.first, len);
        } else {
            memcpy(&key[0], &ek.first, len);
        }
    }

    static void make_key(const char* tkey, Key &key, int len) {
        key.setKeyLen(len);
        if (len > 8) {
            key.set(tkey, len);
        } else {
            memcpy(&key[0], tkey, len);
        }
    }

    // static void freeLeaf(TID t) {
    //     Element* e = (Element*) t;
    //     if (e->key.second > 8) {
    //         Transaction::rcu_free((char*) e->key.first);
    //     }
    //     Transaction::rcu_delete(e);
    // }

    typedef std::tuple<bool, bool, uintptr_t> lookup_return_type; // (success, found, ret)
    typedef std::tuple<bool, bool> ins_return_type; // (success, found)
    typedef std::tuple<bool, bool> del_return_type; // (success, found)

    static constexpr TransItem::flags_type parent_bit = TransItem::user0_bit;
    static constexpr TransItem::flags_type deleted_bit = TransItem::user0_bit<<1;
    static constexpr TransItem::flags_type new_insert_bit = TransItem::user0_bit<<2;
    static constexpr TransItem::flags_type self_upgrade_bit = TransItem::user0_bit<<3;

    TART() {
        root_.access().setLoadKey(TART::loadKey);
    }

    lookup_return_type lookup(lcdf::Str k) {
        Key key;
        make_key(k.data(), key, k.length());
        auto r = root_.access().lookup(key);
        Element* e = (Element*) r.first;
        if (e) {
            auto item = Sto::item(this, e);
            if (item.has_write()) {
                return lookup_return_type(true, true, item.template write_value<TVal>());
            }
            e->vers.lock_exclusive();
            if (e->poisoned) {
                e->vers.unlock_exclusive();
                return lookup_return_type(false, false, 0);
            }
            uintptr_t ret = e->val;
            e->vers.unlock_exclusive();
            if (!item.observe(e->vers)) {
                return lookup_return_type(false, false, 0);
            }
            // e->vers.observe_read(item);
            return lookup_return_type(true, true, ret);
        }
        assert(r.second);
        auto item = Sto::item(this, r.second);
        item.add_flags(parent_bit);
        if (!item.observe(r.second->vers)) {
            return lookup_return_type(false, false, 0);
        }
        // r.second->vers.observe_read(item);
        return lookup_return_type(true, false, 0);
    }
    lookup_return_type lookup(const char* k) {
        return lookup({k, (int) strlen(k)+1});
    }

    TVal transGet(lcdf::Str k) {
        auto r = lookup(k);
        if (!std::get<0>(r)) {
            throw Transaction::Abort();
        }
        return std::get<2>(r);
    }
    TVal transGet(uint64_t k) {
        return transGet({(const char*) &k, sizeof(uint64_t)});
    }
    TVal transGet(const char* k) {
        return transGet({k, (int) strlen(k)+1});
    }

    TVal nonTransGet(lcdf::Str k) {
        Key key;
        make_key(k.data(), key, k.length());
        auto r = root_.access().lookup(key);
        Element* e = (Element*) r.first;
        if (e) {
            return e->val;
        }
        return 0;
    }
    TVal nonTransGet(const char* k) {
        return nonTransGet({k, (int) strlen(k)+1});
    }

    ins_return_type insert(lcdf::Str k, TVal v, bool overwrite) {
        Key art_key;
        make_key(k.data(), art_key, k.length());

        auto r = root_.access().insert(art_key, [k, v]{
            Element* e = new Element();
            if (k.length() > 8) {
                char* p = (char*) malloc(k.length());
                memcpy(p, k.data(), k.length());
                e->key.first = p;
            } else {
                memcpy(&e->key.first, k.data(), k.length());
            }
            e->val = v;
            e->poisoned = true;
            e->key.second = k.length();
            return (TID) e;
        });

        Element* e = (Element*) std::get<0>(r);
        if (!std::get<1>(r)) {
            auto item = Sto::item(this, e);
            e->vers.lock_exclusive();
            if (!item.has_write() && e->poisoned) {
                e->vers.unlock_exclusive();
                return ins_return_type(false, false);
            }
            e->vers.unlock_exclusive();
            item.observe(e->vers);
            // e->vers.observe_read(item);
            if (item.has_flag(deleted_bit)) {
                item.add_write(v);
                item.clear_flags(deleted_bit);
                return ins_return_type(true, false);
            }
            if (overwrite) {
                item.add_write(v);
            }
            return ins_return_type(true, true);
        }

        auto item_el = Sto::item(this, e);
        auto item_parent = Sto::item(this, std::get<1>(r));
        item_parent.add_write(v);
        item_el.add_write(v);
        item_el.add_flags(new_insert_bit);
        item_parent.add_flags(parent_bit);

        Node* old = std::get<2>(r);
        if (old) {
            old->vers.lock_exclusive();
            old->valid = false;
            old->vers.unlock_exclusive();
        }
        return ins_return_type(true, false);
    }
    ins_return_type insert(const char* k, TVal v, bool overwrite) {
        return insert({k, (int) strlen(k)+1}, v, overwrite);
    }

    void transPut(lcdf::Str k, TVal v) {
        Key art_key;
        make_key(k.data(), art_key, k.length());

        auto r = root_.access().insert(art_key, [k, v]{
            Element* e = new Element();
            if (k.length() > 8) {
                char* p = (char*) malloc(k.length());
                memcpy(p, k.data(), k.length());
                e->key.first = p;
            } else {
                memcpy(&e->key.first, k.data(), k.length());
            }
            e->val = v;
            e->poisoned = true;
            e->key.second = k.length();
            return (TID) e;
        });

        Element* e = (Element*) std::get<0>(r);
        if (!std::get<1>(r)) {
            auto item = Sto::item(this, e);
            e->vers.lock_exclusive();
            if (!item.has_write() && e->poisoned) {
                e->vers.unlock_exclusive();
                throw Transaction::Abort();
            }
            e->vers.unlock_exclusive();
            item.add_write(v);
            item.clear_flags(deleted_bit);
            return;
        }

        auto item_el = Sto::item(this, e);
        auto item_parent = Sto::item(this, std::get<1>(r));
        item_parent.add_write(v);
        item_el.add_write(v);
        item_el.add_flags(new_insert_bit);
        item_parent.add_flags(parent_bit);

        Node* old = std::get<2>(r);
        if (old) {
            old->vers.lock_exclusive();
            old->valid = false;
            old->vers.unlock_exclusive();
            Sto::item(this, old).add_flags(self_upgrade_bit);
            item_parent.observe(std::get<1>(r)->vers);
            // std::get<1>(r)->vers.observe_read(item_parent);
        }
    }
    void transPut(uint64_t k, TVal v) {
        transPut({(const char*) &k, sizeof(uint64_t)}, v);
    }
    void transPut(const char* k, TVal v) {
        transPut({k, (int) strlen(k)+1}, v);
    }

    void nonTransPut(lcdf::Str k, TVal v) {
        Key art_key;
        make_key(k.data(), art_key, k.length());
        auto r = root_.access().insert(art_key, [k, v]{
            Element* e = new Element();
            if (k.length() > 8) {
                char* p = (char*) malloc(k.length());
                memcpy(p, k.data(), k.length());
                e->key.first = p;
            } else {
                memcpy(&e->key.first, k.data(), k.length());
            }
            e->val = v;
            e->key.second = k.length();
            return (TID) e;
        });
        Element* e = (Element*) std::get<0>(r);
        if (!std::get<1>(r)) {
            e->val = v;
            return;
        }
    }
    void nonTransPut(const char* k, TVal v) {
        return nonTransPut({k, (int) strlen(k)+1}, v);
    }

    del_return_type remove(lcdf::Str k) {
        Key art_key;
        make_key(k.data(), art_key, k.length());
        auto r = root_.access().lookup(art_key);
        Element* e = (Element*) r.first;
        if (e) {
            auto item = Sto::item(this, e);
            e->vers.lock_exclusive();
            if (!item.has_write() && e->poisoned) {
                e->vers.unlock_exclusive();
                return del_return_type(false, false);
            }
            e->poisoned = true;
            e->vers.unlock_exclusive();
            item.add_write(0);
            item.add_flags(deleted_bit);
            // e->vers.observe_read(item);
            item.observe(e->vers);
            return del_return_type(true, true);
        }

        auto item_parent = Sto::item(this, r.second);
        item_parent.add_flags(parent_bit);
        // r.second->vers.observe_read(item_parent);
        item_parent.observe(r.second->vers);
        return del_return_type(true, false);
    }
    del_return_type remove(const char* k) {
        return remove({k, (int) strlen(k)+1});
    }

    void transRemove(lcdf::Str k) {
        Key art_key;
        make_key(k.data(), art_key, k.length());
        auto r = root_.access().lookup(art_key);
        Element* e = (Element*) r.first;
        if (e) {
            auto item = Sto::item(this, e);
            e->vers.lock_exclusive();
            if (!item.has_write() && e->poisoned) {
                e->vers.unlock_exclusive();
                throw Transaction::Abort();
            }
            e->poisoned = true;
            e->vers.unlock_exclusive();
            item.add_write(0);
            item.add_flags(deleted_bit);
            return;
        }

        auto item_parent = Sto::item(this, r.second);
        item_parent.add_flags(parent_bit);
        // r.second->vers.observe_read(item_parent);
        item_parent.observe(r.second->vers);
    }
    void transRemove(uint64_t k) {
        transRemove({(const char*) &k, sizeof(uint64_t)});
    }
    void transRemove(const char* k) {
        transRemove({k, (int) strlen(k)+1});
    }

    bool lookupRange(TKey start, TKey end, TKey continueKey, TVal result[],
                                std::size_t resultSize, std::size_t &resultsFound) const {
        Key art_start;
        Key art_end;
        Key art_continue;

        art_start.set(start.first, start.second);
        art_end.set(end.first, end.second);
        // art_continue.set(continueKey.first, continueKey.second);

        TID* art_result = new TID[resultSize];

        bool success = root_.access().lookupRange(art_start, art_end, art_continue, art_result, resultSize, resultsFound, [this](Node* node){
            auto item = Sto::item(this, node);
            // node->vers.observe_read(item);
            item.observe(node->vers);
        });
        int validResults = resultsFound;

        int j = 0;
        for (uint64_t i = 0; i < resultsFound; i++) {
            Element* e = (Element*) art_result[i];
            auto item = Sto::item(this, e);
            if (item.has_flag(deleted_bit)) {
                validResults--;
            } else if (item.has_write()) {
                result[j] = item.template write_value<TVal>();
                j++;
            } else if (e->poisoned) {
                throw Transaction::Abort();
            } else {
                item.observe(e->vers);
                // e->vers.observe_read(item);
                result[j] = e->val;
                j++;
            }
        }
        resultsFound = validResults;

        return success;
    }

    bool lock(TransItem& item, Transaction& txn) override {
        if (item.has_flag(parent_bit)) {
            Node* parent = item.template key<Node*>();
            return parent->vers.is_locked_here() || txn.try_lock(item, parent->vers);
        } else {
            Element* e = item.template key<Element*>();
            bool locked = txn.try_lock(item, e->vers);
            if (!locked) {
                return false;
            }
            if (e->poisoned && !(item.has_flag(new_insert_bit) || item.has_flag(deleted_bit))) {
                e->vers.cp_unlock(item);
                return false;
            }
            return true;
        }
    }
    bool check(TransItem& item, Transaction& txn) override {
        if (item.has_flag(parent_bit)) {
            Node* parent = item.template key<Node*>();
            return (item.has_flag(self_upgrade_bit) || parent->valid) && parent->vers.cp_check_version(txn, item);
        } else {
            Element* e = item.template key<Element*>();
            return (!e->poisoned || item.has_flag(new_insert_bit) || item.has_flag(deleted_bit)) && e->vers.cp_check_version(txn, item);
        }
    }
    void install(TransItem& item, Transaction& txn) override {
        if (item.has_flag(parent_bit)) {
            Node* parent = item.template key<Node*>();
            txn.set_version_unlock(parent->vers, item);
        } else {
            Element* e = item.template key<Element*>();
            if (item.has_flag(deleted_bit)) {
                Key art_key;
                // art_key.set(e->key.first.c_str(), e->key.first.size()+1);
                if (e->key.second > 8) {
                    make_key(e->key.first, art_key, e->key.second);
                } else {
                    make_key((const char*) &e->key.first, art_key, e->key.second);
                }
                Node* old = root_.access().remove(art_key, (TID) e);
                if (old) {
                    old->valid = false;
                    Transaction::rcu_delete(old);
                }
                if (e->key.second > 8) {
                    Transaction::rcu_free((char*) e->key.first);
                }
                Transaction::rcu_delete(e);
                return;
            }
            e->poisoned = false;
            e->val = item.template write_value<TVal>();
            txn.set_version_unlock(e->vers, item);
        }
    }
    void unlock(TransItem& item) override {
        if (item.has_flag(parent_bit)) {
            Node* parent = item.template key<Node*>();
            if (parent->vers.is_locked_here()) parent->vers.cp_unlock(item);
        } else {
            Element* e = item.template key<Element*>();
            e->vers.cp_unlock(item);
        }
    }
    void cleanup(TransItem& item, bool committed) override {
        if (committed || item.has_flag(parent_bit) || !item.has_write()) {
            return;
        }
        Element* e = item.template key<Element*>();
        if (item.has_flag(deleted_bit)) {
            e->poisoned = false;
            return;
        }
        if (e->poisoned && item.has_flag(new_insert_bit)) {
            Key art_key;
            if (e->key.second > 8) {
                make_key(e->key.first, art_key, e->key.second);
            } else {
                make_key((const char*) &e->key.first, art_key, e->key.second);
            }
            Node* old = root_.access().remove(art_key, (TID) e);
            if (old) {
                old->valid = false;
                Transaction::rcu_delete(old);
            }
            if (e->key.second > 8) {
                Transaction::rcu_free((char*) e->key.first);
            }
            Transaction::rcu_delete(e);
        }
    }
    void print(std::ostream& w, const TransItem& item) const override {
        root_.access().print();
    }
    
    void print() const {
        root_.access().print();
    }
protected:
    TOpaqueWrapped<ART_OLC::Tree> root_;
};
