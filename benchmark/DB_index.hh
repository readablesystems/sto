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
    typedef typename IndexType::value_type value_type;

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
    constexpr static std::array<access_t, T::num_splits>
    mvcc_column_to_cell_accesses(std::initializer_list<column_access_t> accesses) {
        constexpr size_t num_splits = T::num_splits;
        std::array<access_t, num_splits> cell_accesses { access_t::none };

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

    // mvcc_*_loop() methods: Template meta-programs that iterates through all version "splits" at compile time.
    // Template parameters:
    // C: size of split (should be constant throughout the recursive instantiation)
    // I: Split index (for iteration)
    // First: Type of the first split column
    // Rest...: Type(s) of the rest of the split columns (could be empty)
    // Return value: success (false == abort)
    template <int C, int I, typename First, typename... Rest>
    static bool
    mvcc_select_loop(const std::array<access_t, C>& cell_accesses,
                  std::array<void*, C>& value_ptrs,
                  TObject* tobj, internal_elem* e) {
        auto mvobj = e->template chain_at<I>();
        if (cell_accesses[I] != access_t::none) {
            auto item = Sto::item(tobj, item_key_t(e, I));
            auto h = mvobj->find(Sto::read_tid<IndexType::Commute>());
            if (!h->status_is(UNUSED) && !IndexType::template is_phantom<First>(h, item)) {
                // XXX No read-my-write stuff for now.
                MvAccess::template read<First>(item, h);
#if SAFE_FLATTEN
                auto vp = h->vp_safe_flatten();
                if (vp == nullptr)
                    return false;
                value_ptrs[I] = vp;
#else
                auto vp = h->vp();
                assert(vp);
                value_ptrs[I] = vp;
#endif
            }
        }
        return mvcc_select_loop<C, I + 1, Rest...>(cell_accesses, value_ptrs, tobj, e);
    }

    template <int C, int I>
    static bool
    mvcc_select_loop(const std::array<access_t, C>&, std::array<void*, C>&, TObject*, internal_elem*) {
        static_assert(I == C, "Index invalid.");
        return true;
    }

    // Generate loops for non-trans access. P is SplitParams.
    template <int C, int I, typename P, typename First, typename... Rest>
    static void
    mvcc_nontrans_put_loop(const value_type& whole_value, internal_elem* e) {
        e->template chain_at<I>()->nontrans_access() = std::get<I>(P::split_builder)(whole_value);
        return mvcc_nontrans_put_loop<C, I+1, P, Rest...>(whole_value, e);
    }
    template <int C, int I, typename P>
    static void
    mvcc_nontrans_put_loop(const value_type&, internal_elem*) {
        static_assert(I == C, "Index invalid.");
    }
    template <int C, int I, typename P, typename First, typename... Rest>
    static void
    mvcc_nontrans_get_loop(value_type* whole_value_out, internal_elem* e) {
        const First* chain_val = &(e->template chain_at<I>()->nontrans_access());
        std::get<I>(P::split_merger)(whole_value_out, chain_val);
        return mvcc_nontrans_get_loop<C, I+1, P, Rest...>(whole_value_out, e);
    }
    template <int C, int I, typename P>
    static void
    mvcc_nontrans_get_loop(value_type*, internal_elem*) {
        static_assert(I == C, "Index invalid.");
    }

    // Static looping for TObject::lock
    template <int C, int I, typename First, typename... Rest>
    static bool mvcc_lock_loop(int cell_id, Transaction& txn, TransItem& item, IndexType* idx, internal_elem* e) {
        if (cell_id == I) {
            auto e = item.key<item_key_t>().internal_elem_ptr();
            return idx->lock_impl_per_chain(item, txn, e->template chain_at<I>(), e);
        }
        return mvcc_lock_loop<C, I+1, Rest...>(cell_id, txn, item, idx, e);
    }
    template<int C, int I>
    static bool mvcc_lock_loop(int, Transaction&, TransItem&, IndexType*, internal_elem*) {
        static_assert(C == I, "Index invalid");
        always_assert(false, "One past last iteration should never execute.");
        return false;
    }
    // Static looping for TObject::check
    template <int C, int I, typename First, typename... Rest>
    static bool mvcc_check_loop(int cell_id, Transaction& txn, TransItem& item, IndexType* idx) {
        if (cell_id == I) {
            auto e = item.key<item_key_t>().internal_elem_ptr();
            return idx->check_impl_per_chain(item, txn, e->template chain_at<I>());
        }
        return mvcc_check_loop<C, I+1, Rest...>(cell_id, txn, item, idx);
    }
    template<int C, int I>
    static bool mvcc_check_loop(int, Transaction&, TransItem&, IndexType*) {
        static_assert(C == I, "Index invalid");
        always_assert(false, "One past last iteration should never execute.");
        return false;
    }
    // Static looping for TObject::install
    template <int C, int I, typename First, typename... Rest>
    static void mvcc_install_loop(int cell_id, Transaction& txn, TransItem& item, IndexType* idx) {
        if (cell_id == I) {
            auto e = item.key<item_key_t>().internal_elem_ptr();
            idx->install_impl_per_chain(item, txn, e->template chain_at<I>());
            return;
        }
        mvcc_install_loop<C, I+1, Rest...>(cell_id, txn, item, idx);
    }
    template<int C, int I>
    static void mvcc_install_loop(int, Transaction&, TransItem&, IndexType*) {
        static_assert(C == I, "Index invalid");
        always_assert(false, "One past last iteration should never execute.");
    }
    // Static looping for TObject::cleanup
    template <int C, int I, typename First, typename... Rest>
    static void mvcc_cleanup_loop(int cell_id, TransItem& item, bool committed, IndexType* idx) {
        if (cell_id == I) {
            auto e = item.key<item_key_t>().internal_elem_ptr();
            idx->cleanup_impl_per_chain(item, committed, e->template chain_at<I>());
            return;
        }
        mvcc_cleanup_loop<C, I+1, Rest...>(cell_id, item, committed, idx);
    }
    template<int C, int I>
    static void mvcc_cleanup_loop(int, TransItem&, bool, IndexType*) {
        static_assert(C == I, "Index invalid");
        always_assert(false, "One past last iteration should never execute.");
    }

    // Helper struct unwrapping tuples into template parameter packs.
    template <typename P, typename Tuple = typename P::split_type_list>
    struct MvSplitAccessAll;
    template <typename P, typename... SplitTypes>
    struct MvSplitAccessAll<P, std::tuple<SplitTypes...>> {
        static std::array<void*, P::num_splits> run_select(
                const std::array<access_t, P::num_splits>& cell_accesses,
                TObject* tobj,
                internal_elem* e) {
            std::array<void*, P::num_splits> value_ptrs = { nullptr };
            mvcc_select_loop<P::num_splits, 0, SplitTypes...>(cell_accesses, value_ptrs, tobj, e);
            return value_ptrs;
        }
        static void run_nontrans_put(const value_type& whole_value, internal_elem* e) {
            mvcc_nontrans_put_loop<P::num_splits, 0, P, SplitTypes...>(whole_value, e);
        }
        static void run_nontrans_get(value_type* whole_value_out, internal_elem* e) {
            mvcc_nontrans_get_loop<P::num_splits, 0, P, SplitTypes...>(whole_value_out, e);
        }
        static bool run_lock(int cell_id, Transaction& txn, TransItem& item, IndexType* idx, internal_elem* e) {
            return mvcc_lock_loop<P::num_splits, 0, SplitTypes...>(cell_id, txn, item, idx, e);
        }
        static bool run_check(int cell_id, Transaction& txn, TransItem& item, IndexType* idx) {
            return mvcc_check_loop<P::num_splits, 0, SplitTypes...>(cell_id, txn, item, idx);
        }
        static void run_install(int cell_id, Transaction& txn, TransItem& item, IndexType* idx) {
            mvcc_install_loop<P::num_splits, 0, SplitTypes...>(cell_id, txn, item, idx);
        }
        static void run_cleanup(int cell_id, TransItem& item, bool committed, IndexType* idx) {
            mvcc_cleanup_loop<P::num_splits, 0, SplitTypes...>(cell_id, item, committed, idx);
        }
    };
};

// MVCC split version!

// Helper class that turns a tuple<A, B, ...> into
// tuple<MvObject<A>, MvObject<B>, ...>
template <typename Tuple>
struct SplitMvObjectBuilder;
template <class... T>
struct SplitMvObjectBuilder<std::tuple<T...>> {
    using type = std::tuple<MvObject<T>...>;
};

// MVCC default split parameters
template <typename RowType>
struct SplitParams {
    // These are auto-generated or user-specified.
    using split_type_list = std::tuple<RowType>;
    static constexpr auto split_builder = std::make_tuple(
        [](const RowType& in) -> RowType {return in;}
    );
    static constexpr auto split_merger = std::make_tuple(
        [](RowType* out, const RowType* in) -> void {*out = *in;}
    );
    // MVCC split IDs are 1-indexed (we don't do the major/minor version thing in OCC here.)
    static constexpr auto map = [](int) -> int {return 1;};

    // These need not change.
    using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
    static constexpr size_t num_splits = std::tuple_size<layout_type>::value;
};

template <typename A, typename V>
class RecordAccessor;

template <typename V>
class UniRecordAccessor;

template <typename V>
class SplitRecordAccessor;

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
