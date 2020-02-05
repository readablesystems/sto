#pragma once
#include "config.h"
#include "compiler.hh"

#include "Sto.hh"

#include "masstree.hh"
#include "kvthread.hh"
#include "masstree_tcursor.hh"
#include "masstree_insert.hh"
#include "masstree_print.hh"
#include "masstree_remove.hh"
#include "masstree_scan.hh"
#include "string.hh"

#include <vector>
#include "DB_structs.hh"
#include "VersionSelector.hh"
#include "MVCC.hh"

#include "TBox.hh"
#include "TMvBox.hh"

namespace bench {

class version_adapter {
public:
    template <bool Adaptive>
    static bool select_for_update(TransProxy& item, TLockVersion<Adaptive>& vers) {
        return item.acquire_write(vers);
    }
    static bool select_for_update(TransProxy& item, TVersion& vers) {
        if (!item.observe(vers))
            return false;
        item.add_write();
        return true;
    }
    static bool select_for_update(TransProxy& item, TNonopaqueVersion& vers) {
        if (!item.observe(vers))
            return false;
        item.add_write();
        return true;
    }
    template <bool Opaque>
    static bool select_for_update(TransProxy& item, TSwissVersion<Opaque>& vers) {
        return item.acquire_write(vers);
    }
    // XXX is it correct?
    template <bool Opaque, bool Extend>
    static bool select_for_update(TransProxy& item, TicTocVersion<Opaque, Extend>& vers) {
        if (!item.observe(vers))
            return false;
        item.acquire_write(vers);
        return true;
    }

    template <bool Adaptive, typename T>
    static bool select_for_overwrite(TransProxy& item, TLockVersion<Adaptive>& vers, const T& val) {
        return item.acquire_write(vers, val);
    }
    template <typename T>
    static bool select_for_overwrite(TransProxy& item, TVersion& vers, const T& val) {
        (void)vers;
        item.add_write(val);
        return true;
    }
    template <typename T>
    static bool select_for_overwrite(TransProxy& item, TNonopaqueVersion& vers, const T& val) {
        (void)vers;
        item.add_write(val);
        return true;
    }
    template <bool Opaque, typename T>
    static bool select_for_overwrite(TransProxy& item, TSwissVersion<Opaque>& vers, const T& val) {
        return item.acquire_write(vers, val);
    }
    template <bool Opaque, bool Extend, typename T>
    static bool select_for_overwrite(TransProxy& item, TicTocVersion<Opaque, Extend>& vers, const T& val) {
        return item.acquire_write(vers, val);
    }
};

template <typename DBParams>
struct get_occ_version {
    typedef typename std::conditional<DBParams::Opaque, TVersion, TNonopaqueVersion>::type type;
};

template <typename DBParams>
struct get_version {
    typedef typename std::conditional<DBParams::MVCC, TNonopaqueVersion,
            typename std::conditional<DBParams::TicToc, TicTocVersion<>,
            typename std::conditional<DBParams::Adaptive, TLockVersion<true /* adaptive */>,
            typename std::conditional<DBParams::TwoPhaseLock, TLockVersion<false>,
            typename std::conditional<DBParams::Swiss, TSwissVersion<DBParams::Opaque>,
            typename get_occ_version<DBParams>::type>::type>::type>::type>::type>::type type;
};

template <typename DBParams>
class TCommuteIntegerBox : public TObject {
public:
    typedef int64_t int_type;
    typedef typename get_version<DBParams>::type version_type;

    TCommuteIntegerBox()
        : vers(Sto::initialized_tid() | TransactionTid::nonopaque_bit),
          value() {}

    TCommuteIntegerBox& operator=(int_type x) {
        value = x;
        return *this;
    }

    std::pair<bool, int_type> read() {
        auto item = Sto::item(this, 0);
        bool ok = item.observe(vers);
        if (ok)
            return {true, value};
        else
            return {false, 0};
    }

    void increment(int_type i) {
        auto item = Sto::item(this, 0);
        item.acquire_write(vers, i);
    }

    bool lock(TransItem& item, Transaction& txn) override {
        return txn.try_lock(item, vers);
    }
    bool check(TransItem& item, Transaction& txn) override {
        return vers.cp_check_version(txn, item);
    }
    void install(TransItem& item, Transaction& txn) override {
        value += item.write_value<int_type>();
        txn.set_version_unlock(vers, item);
    }
    void unlock(TransItem& item) override {
        vers.cp_unlock(item);
    }

private:

    version_type vers;
    int_type value;
};

template <typename DBParams>
struct integer_box {
    typedef typename std::conditional<DBParams::MVCC,
        TMvCommuteIntegerBox, TCommuteIntegerBox<DBParams>>::type type;
};

// Row/column access specifiers and split version helpers (OCC-only)
enum class RowAccess : int { None = 0, ObserveExists, ObserveValue, UpdateValue };

enum class access_t : int8_t { none = 0, read = 1, write = 2, update = 3 };

template <typename IndexType>
class split_version_helpers {
public:
    typedef typename IndexType::NamedColumn NamedColumn;
    typedef typename IndexType::internal_elem internal_elem;

    struct column_access_t {
        int col_id;
        access_t access;

        column_access_t(NamedColumn column, access_t access)
                : col_id(static_cast<int>(column)), access(access) {}
    };

    // TransItem key format:
    // |----internal_elem pointer----|--cell id--|I|
    //           48 bits                14 bits   2

    // I: internode and ttnv bits (or bucket bit in the unordered case)
    // cell id: valid range 0-16383 (0x3fff)
    // cell id 0 identifies the row item

    class item_key_t {
        typedef uintptr_t type;
        static constexpr unsigned shift = 16u;
        static constexpr type cell_mask = type(0xfffc);
        static constexpr int row_item_cell_num = 0x0;
        type key_;

    public:
        item_key_t() : key_() {};

        item_key_t(internal_elem *e, int cell_num) : key_((reinterpret_cast<type>(e) << shift)
                                                          | ((static_cast<type>(cell_num) << 2) & cell_mask)) {};

        static item_key_t row_item_key(internal_elem *e) {
            return item_key_t(e, row_item_cell_num);
        }

        internal_elem *internal_elem_ptr() const {
            return reinterpret_cast<internal_elem *>(key_ >> shift);
        }

        int cell_num() const {
            return static_cast<int>((key_ & cell_mask) >> 2);
        }

        bool is_row_item() const {
            return (cell_num() == row_item_cell_num);
        }
    };

    template <typename T>
    constexpr static std::array<access_t, T::num_versions>
    column_to_cell_accesses(std::initializer_list<column_access_t> accesses) {
        constexpr size_t num_versions = T::num_versions;
        std::array<access_t, num_versions> cell_accesses { access_t::none };

        for (auto it = accesses.begin(); it != accesses.end(); ++it) {
            int cell_id = T::map(it->col_id);
            cell_accesses[cell_id]  = static_cast<access_t>(
                    static_cast<uint8_t>(cell_accesses[cell_id]) | static_cast<uint8_t>(it->access));
        }
        return cell_accesses;
    }

    template <typename T>
    static std::pair<bool, std::array<TransItem*, T::num_versions>>
    extract_item_list(const std::array<access_t, T::num_versions>& cell_accesses, TObject *tobj, internal_elem *e) {
        bool any_has_write = false;
        std::array<TransItem*, T::num_versions> cell_items { nullptr };
        for (size_t i = 0; i < T::num_versions; ++i) {
            if (cell_accesses[i] != access_t::none) {
                auto item = Sto::item(tobj, item_key_t(e, i));
                if (IndexType::index_read_my_write && !any_has_write && item.has_write())
                    any_has_write = true;
                cell_items[i] = &item.item();
            }
        }
        return { any_has_write, cell_items };
    }

};

template <typename K, typename V, typename DBParams>
class index_common {
public:
    typedef K key_type;
    typedef V value_type;

    static constexpr TransItem::flags_type insert_bit = TransItem::user0_bit;
    static constexpr TransItem::flags_type delete_bit = TransItem::user0_bit<<1;
    static constexpr TransItem::flags_type row_update_bit = TransItem::user0_bit << 2u;
    static constexpr TransItem::flags_type row_cell_bit = TransItem::user0_bit << 3u;
    // tag TItem key for special treatment
    static constexpr uintptr_t item_key_tag = 1;

    typedef typename value_type::NamedColumn NamedColumn;
    typedef typename get_version<DBParams>::type version_type;
    typedef IndexValueContainer<V, version_type> value_container_type;
    typedef commutators::Commutator<value_type> comm_type;

    typedef std::tuple<bool, bool, uintptr_t, const value_type*> sel_return_type;
    typedef std::tuple<bool, bool>                               ins_return_type;
    typedef std::tuple<bool, bool>                               del_return_type;

    static constexpr sel_return_type sel_abort = { false, false, 0, nullptr };
    static constexpr ins_return_type ins_abort = { false, false };
    static constexpr del_return_type del_abort = { false, false };

    static constexpr typename version_type::type invalid_bit = TransactionTid::user_bit;

    static constexpr bool index_read_my_write = DBParams::RdMyWr;

    static bool has_insert(const TransItem& item) {
        return (item.flags() & insert_bit) != 0;
    }
    static bool has_delete(const TransItem& item) {
        return (item.flags() & delete_bit) != 0;
    }
    static bool has_row_update(const TransItem& item) {
        return (item.flags() & row_update_bit) != 0;
    }
    static bool has_row_cell(const TransItem& item) {
        return (item.flags() & row_cell_bit) != 0;
    }
};

}; // namespace bench

#include "DB_uindex.hh"
#include "DB_oindex.hh"
