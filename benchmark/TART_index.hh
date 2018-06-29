#pragma once
#include "config.h"
#include "compiler.hh"

#include "Sto.hh"

#include "string.hh"
#include "TART.hh"
#include <type_traits>

#include <vector>
#include "VersionSelector.hh"

namespace bench {
template <typename K, typename V, typename DBParams>
class tart_index : public TObject {
public:
    typedef K key_type;
    typedef V value_type;

    typedef std::tuple<bool, bool, uintptr_t, const value_type> sel_return_type;
    typedef std::tuple<bool, bool>                              ins_return_type;
    typedef std::tuple<bool, bool>                              del_return_type;
    
    tart_index() {
        art = TART();
        static_assert(std::is_base_of<std::string, K>::value, "key must be std::string");
        //static_assert(std::is_base_of<uintptr_t, V>::value, "value must be uintptr_t");
    }
    ~tart_index() {}

    // DB operations
    sel_return_type select_row(const key_type& k) {
        auto ret = art.lookup(k);
        return sel_return_type(true, true, 0, ret);
    }
    ins_return_type insert_row(const key_type& k, value_type& v, bool overwrite = false) {
        (void)overwrite;
        art.insert(k, v);
        return ins_return_type(true, false);
    }
    void update_row(const key_type& k, value_type& v) {
        art.insert(k, v);
    }
    del_return_type delete_row(const key_type& k) {
        art.erase(k);
        return del_return_type(true, true);
    }

    value_type nontrans_get(const key_type& k) {
        return art.nonTransGet(k);
    }
    void nontrans_put(const key_type& k, const value_type& v) {
        art.nonTransPut(k, v);
    }

    bool lock(TransItem& item, Transaction& txn) {
        return art.lock(item, txn);
    }
    bool check(TransItem& item, Transaction& txn) {
        return art.check(item, txn);
    }
    void install(TransItem& item, Transaction& txn) {
        art.install(item, txn);
    }
    void unlock(TransItem& item) {
        art.unlock(item);
    }
private:
    TART art;
};
}
