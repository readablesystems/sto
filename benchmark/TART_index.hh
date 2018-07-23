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

    typedef std::tuple<bool, bool, uintptr_t, const value_type*> sel_return_type;
    typedef std::tuple<bool, bool>                              ins_return_type;
    typedef std::tuple<bool, bool>                              del_return_type;
    typedef typename value_type::NamedColumn NamedColumn;

    uint64_t key_gen_;
    uint64_t gen_key() {
        return fetch_and_add(&key_gen_, 1);
    }

	struct column_access_t {
		int col_id;
		bool update;

		column_access_t(NamedColumn column, bool for_update)
				: col_id(static_cast<int>(column)), update(for_update) {}
	};

	struct cell_access_t {
		int cell_id;
		bool update;

		cell_access_t(int cid, bool for_update)
				: cell_id(cid), update(for_update) {}
	};
    
    tart_index() {
        art = new TART();
        key_gen_ = 0;
        // static_assert(std::is_base_of<std::string, K>::value, "key must be std::string");
    }
    ~tart_index() {}

    void thread_init() {}

    // DB operations
    sel_return_type select_row(const key_type& k, RowAccess acc) {
        auto ret = (const value_type*) art->transGet(k);
        if (!ret) {
            return sel_return_type(true, false, 0, 0);
        }
        return sel_return_type(true, true, 0, ret);
    }
    ins_return_type insert_row(const key_type& k, value_type* v, bool overwrite = false) {
        (void)overwrite;
        art->transPut(k, (uintptr_t) v);
        return ins_return_type(true, false);
    }
    // void update_row(const key_type& k, value_type& v) {
    //     lcdf::Str s = k;
    //     art->insert({s.data(), s.length()}, (uintptr_t) &v);
    // }
    void update_row(uintptr_t rid, value_type* v) { }
    del_return_type delete_row(const key_type& k) {
        art->transRemove(k);
        return del_return_type(true, true);
    }

    value_type nontrans_get(const key_type& k) {
        return art->nonTransGet(k);
    }
    void nontrans_put(const key_type& k, const value_type& v) {
        art->nonTransPut(k, (uintptr_t) &v);
    }

    bool lock(TransItem& item, Transaction& txn) {
        return art->lock(item, txn);
    }
    bool check(TransItem& item, Transaction& txn) {
        return art->check(item, txn);
    }
    void install(TransItem& item, Transaction& txn) {
        art->install(item, txn);
    }
    void unlock(TransItem& item) {
        art->unlock(item);
    }
private:
    TART* art;
};
}
