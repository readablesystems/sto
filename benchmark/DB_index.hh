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

inline access_t operator&(const access_t& lhs, const access_t& rhs) {
    return static_cast<access_t>(static_cast<int8_t>(lhs) & static_cast<int8_t>(rhs));
}

template <typename IndexType>
class split_version_helpers {
public:
    typedef typename IndexType::NamedColumn NamedColumn;
    typedef typename IndexType::internal_elem internal_elem;
    typedef typename IndexType::value_type value_type;
    typedef typename commutators::Commutator<value_type> value_comm_type;

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
        std::fill(cell_accesses.begin(), cell_accesses.end(), access_t::none);

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
        std::fill(cell_accesses.begin(), cell_accesses.end(), access_t::none);

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
        std::fill(cell_items.begin(), cell_items.end(), nullptr);

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
    // Return value: success (false == not found)
    template <int C, int I, typename First, typename... Rest>
    static bool
    mvcc_select_loop(const std::array<access_t, C>& cell_accesses,
                  std::array<void*, C>& value_ptrs,
                  TObject* tobj, internal_elem* e) {
        auto mvobj = e->template chain_at<I>();
        if (cell_accesses[I] != access_t::none) {
            auto item = Sto::item(tobj, item_key_t(e, I));
            if ((cell_accesses[I] & access_t::write) != access_t::none) {
                item.add_write();
            }
            if ((cell_accesses[I] & access_t::read) != access_t::none) {
                auto h = mvobj->find(Sto::read_tid());
                if (h->status_is(COMMITTED_DELETED)) {
                    MvAccess::template read<First>(item, h);
                    return false;
                } else if (!IndexType::template is_phantom<First>(h, item)) {
                    // XXX No read-my-write stuff for now.
                    MvAccess::template read<First>(item, h);

                    auto vp = h->vp();
                    assert(vp);
                    value_ptrs[I] = vp;
                }
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

    // Generate loops for range scan's "value callback". The user-supplied callback for the range scan
    // operates on accessor objects.
    template <int C, int I, typename First, typename... Rest>
    static void
    mvcc_scan_loop(bool& ret, bool& count,
                   const std::array<access_t, C>& cell_accesses,
                   TObject* tobj, internal_elem* e,
                   std::array<void*, C>& split_values) {
        auto mvobj = e->template chain_at<I>();

        auto h = mvobj->find(Sto::read_tid());

        /*
        if (I == 0) {
            // skip invalid (inserted but yet committed) and/or deleted values, but do not abort
            if (h->status_is(DELETED)) {
                count = false;
                return;
            }
        }
        */

        TransProxy row_item =
            IndexType::index_read_my_write
                ? Sto::item(tobj, item_key_t(e, I))
                : Sto::fresh_item(tobj, item_key_t(e, I));

        if (I == 0) {
            if (h->status_is(DELETED)) {
                MvAccess::template read<First>(row_item, h);
                count = false;
                return;
            } else if (IndexType::index_read_my_write) {
                if (IndexType::has_delete(row_item)) {
                    count = false;
                    return;
                }
                if (IndexType::has_row_update(row_item)) {
                    split_values[I] = row_item.template raw_write_value<First*>();
                    mvcc_scan_loop<C, I + 1, Rest...>(ret, count, cell_accesses, tobj, e, split_values);
                    return;
                }
            }
        }

        if (cell_accesses[I] != access_t::none) {
            MvAccess::template read<First>(row_item, h);
            auto vptr = h->vp();
            //ret &= callback(IndexType::key_type(key), *vptr);
            split_values[I] = vptr;
        }
        mvcc_scan_loop<C, I + 1, Rest...>(ret, count, cell_accesses, tobj, e, split_values);
    }

    template <int C, int I>
    static bool
    mvcc_scan_loop(bool&, bool&, const std::array<access_t, C>&, TObject*, internal_elem*, std::array<void*, C>&) {
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
            return idx->lock_impl_per_chain(item, txn, e->template chain_at<I>());
        }
        return mvcc_lock_loop<C, I+1, Rest...>(cell_id, txn, item, idx, e);
    }
    template<int C, int I>
    static bool mvcc_lock_loop(int cell_id, Transaction&, TransItem&, IndexType*, internal_elem*) {
        static_assert(C == I, "Index invalid");
        always_assert(!cell_id, "One past last iteration should never execute.");
        return true;
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
    static bool mvcc_check_loop(int cell_id, Transaction&, TransItem&, IndexType*) {
        static_assert(C == I, "Index invalid");
        always_assert(!cell_id, "One past last iteration should never execute.");
        return true;
    }
    // Static looping for TObject::install
    template <int C, int I, typename First, typename... Rest>
    static void mvcc_install_loop(int cell_id, Transaction& txn, TransItem& item, IndexType* idx, void (*dcb)(void*)) {
        if (cell_id == I) {
            auto key = item.key<item_key_t>();
            auto e = key.internal_elem_ptr();
            idx->install_impl_per_chain(item, txn, e->template chain_at<I>(),
                    key.is_row_item() ? dcb : nullptr);
            return;
        }
        mvcc_install_loop<C, I+1, Rest...>(cell_id, txn, item, idx, dcb);
    }
    template<int C, int I>
    static void mvcc_install_loop(int cell_id, Transaction&, TransItem&, IndexType*, void (*)(void*)) {
        static_assert(C == I, "Index invalid");
        always_assert(!cell_id, "One past last iteration should never execute.");
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
    static void mvcc_cleanup_loop(int cell_id, TransItem&, bool, IndexType*) {
        static_assert(C == I, "Index invalid");
        always_assert(!cell_id, "One past last iteration should never execute.");
    }
    // Static looping for update_row operations.
    // For value updates (non-commutative)
    template <typename P, int C, int I, typename First, typename... Rest>
    static void mvcc_update_loop(IndexType* idx, internal_elem* e, value_type* whole_value_in) {
        auto[found, item] = Sto::find_write_item(idx, item_key_t(e, I));
        if (found) {
            item.clear_write();
            if constexpr (std::is_same<First, value_type>::value) {
                item.add_write(whole_value_in);
            } else {
                auto p = Sto::tx_alloc<First>();
                *p = std::get<I>(P::split_builder)(*whole_value_in);
                item.add_write(p);
            }
        }
        mvcc_update_loop<P, C, I+1, Rest...>(idx, e, whole_value_in);
    }
    template <typename P, int C, int I>
    static void mvcc_update_loop(IndexType*, internal_elem*, value_type*) {
        static_assert(C == I, "Index invalid");
    }
    // For commutative updates
    template <int C, int I, typename First, typename... Rest>
    static void mvcc_update_loop(IndexType* idx, internal_elem* e, const value_comm_type& comm) {
        auto[found, item] = Sto::find_write_item(idx, item_key_t(e, I));
        if (found) {
            item.add_commute(static_cast<commutators::Commutator<First>>(comm));
        }
        mvcc_update_loop<C, I+1, Rest...>(idx, e, comm);
    }
    template <int C, int I>
    static void mvcc_update_loop(IndexType*, internal_elem*, const value_comm_type&) {
        static_assert(C == I, "Index invalid");
    }

    // Helper struct unwrapping tuples into template parameter packs.
    template <typename P, typename Tuple = typename P::split_type_list>
    struct MvSplitAccessAll;
    template <typename P, typename... SplitTypes>
    struct MvSplitAccessAll<P, std::tuple<SplitTypes...>> {
        static std::array<void*, P::num_splits> run_select(
                bool* found,
                const std::array<access_t, P::num_splits>& cell_accesses,
                TObject* tobj,
                internal_elem* e) {
            std::array<void*, P::num_splits> value_ptrs = { nullptr };
            *found = mvcc_select_loop<P::num_splits, 0, SplitTypes...>(cell_accesses, value_ptrs, tobj, e);
            return value_ptrs;
        }
        template <typename Callback>
        static bool run_scan_callback(
                bool& ret,
                bool& count,
                const std::array<access_t, P::num_splits>& cell_accesses,
                const lcdf::Str& key,
                TObject* tobj,
                internal_elem* e,
                Callback callback) {
            ret = true;
            count = true;
            std::array<void*, P::num_splits> split_values = { nullptr };
            mvcc_scan_loop<P::num_splits, 0, SplitTypes...>(ret, count, cell_accesses, tobj, e, split_values);

            if (ret && count) {
                return callback((typename IndexType::key_type)(key), split_values);
            } else {
                return ret;
            }
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
        static void run_install(int cell_id, Transaction& txn, TransItem& item, IndexType* idx, void (*dcb)(void*)) {
            mvcc_install_loop<P::num_splits, 0, SplitTypes...>(cell_id, txn, item, idx, dcb);
        }
        static void run_cleanup(int cell_id, TransItem& item, bool committed, IndexType* idx) {
            mvcc_cleanup_loop<P::num_splits, 0, SplitTypes...>(cell_id, item, committed, idx);
        }
        static void run_update(IndexType* idx, internal_elem* e, value_type* whole_value_in) {
            mvcc_update_loop<P, P::num_splits, 0, SplitTypes...>(idx, e, whole_value_in);
        }
        static void run_update(IndexType* idx, internal_elem* e, const value_comm_type& comm) {
            mvcc_update_loop<P::num_splits, 0, SplitTypes...>(idx, e, comm);
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
    static constexpr auto map = [](int) -> int {return 0;};

    // These need not change.
    using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
    static constexpr size_t num_splits = std::tuple_size<layout_type>::value;
};

// Helper method, turning a user-provided row into split row representation (array of pointers)
// allocated in the transactional scratch space.
template <size_t C, size_t I, typename RowType>
void TxSplitIntoHelper(std::array<void*, C>& value_ptrs, const RowType* whole_row) {
    if constexpr (I == C) {
        (void)whole_row;
        return;
    } else {
        auto p = Sto::tx_alloc<std::tuple_element_t<I, typename SplitParams<RowType>::split_type_list>>();
        *p = std::get<I>(SplitParams<RowType>::split_builder)(*whole_row);
        value_ptrs[I] = p;

        TxSplitIntoHelper<C, I+1, RowType>(value_ptrs, whole_row);
    }
}

template<typename RowType>
std::array<void*, SplitParams<RowType>::num_splits> TxSplitInto(const RowType* whole_row) {
    std::array<void*, SplitParams<RowType>::num_splits> value_ptrs;
    TxSplitIntoHelper<SplitParams<RowType>::num_splits, 0, RowType>(value_ptrs, whole_row);
    return value_ptrs;
}

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

    static constexpr bool Commute = DBParams::Commute;

    typedef typename value_type::NamedColumn NamedColumn;
    typedef typename get_version<DBParams>::type version_type;
    typedef IndexValueContainer<V, version_type> value_container_type;
    typedef commutators::Commutator<value_type> comm_type;

    typedef typename std::conditional_t<DBParams::MVCC,
      SplitRecordAccessor<V>, UniRecordAccessor<V>> accessor_t;
    typedef typename std::conditional_t<DBParams::MVCC,
      void*, std::array<void*, SplitParams<V>::num_splits>> scan_value_t;

    typedef std::tuple<bool, bool, uintptr_t, const value_type*>  sel_return_type;
    typedef std::tuple<bool, bool>                                ins_return_type;
    typedef std::tuple<bool, bool>                                del_return_type;
    typedef std::tuple<bool, bool, uintptr_t, accessor_t>         sel_split_return_type;

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

    struct MvInternalElement {
        typedef typename SplitParams<value_type>::layout_type split_layout_type;
        using object0_type = std::tuple_element_t<0, split_layout_type>;

        split_layout_type split_row;
        key_type key;
        void* table;

        MvInternalElement(void* t, const key_type& k)
            : split_row(), key(k), table(t) {
        }

        template <typename... Args>
        MvInternalElement(void* t, const key_type& k, Args... args)
            : split_row(std::forward<Args>(args)...), key(k), table(t) {
        }

        template <int I>
        std::tuple_element_t<I, split_layout_type>* chain_at() {
            return &std::get<I>(split_row);
        }

        static intptr_t row_chain_offset() {
            void* base = nullptr;
            const auto base_address = reinterpret_cast<intptr_t>(base);
            const auto chain_address = reinterpret_cast<intptr_t>(
                    reinterpret_cast<MvInternalElement*>(base)->chain_at<0>());
            return chain_address - base_address;
        }

        static MvInternalElement* from_chain(std::tuple_element_t<0, split_layout_type>* chain) {
            return reinterpret_cast<MvInternalElement*>(
                    reinterpret_cast<intptr_t>(chain) - row_chain_offset());
        }
    };
};

template <typename K, typename V, typename DBParams>
class mvcc_chain_operations {
public:
    using internal_elem = typename index_common<K, V, DBParams>::MvInternalElement;

    static bool has_delete(const TransItem& item) {
        return index_common<K, V, DBParams>::has_delete(item);
    }

    template <typename TSplit>
    static bool lock_impl_per_chain(TransItem& item, Transaction& txn, MvObject<TSplit>* chain);
    template <typename TSplit>
    static bool check_impl_per_chain(TransItem& item, Transaction& txn, MvObject<TSplit>* chain);
    template <typename TSplit>
    static void install_impl_per_chain(TransItem& item, Transaction& txn, MvObject<TSplit>* chain, void (*dcb)(void*));
    template <typename TSplit>
    static void cleanup_impl_per_chain(TransItem& item, bool committed, MvObject<TSplit>* chain);
};

template <typename K, typename V, typename DBParams>
template <typename TSplit>
bool mvcc_chain_operations<K, V, DBParams>::lock_impl_per_chain(
        TransItem& item, Transaction& txn, MvObject<TSplit>* chain) {
    using history_type = typename MvObject<TSplit>::history_type;
    using comm_type = typename commutators::Commutator<TSplit>;

    if (item.has_read()) {
        auto hprev = item.read_value<history_type*>();
        if (Sto::commit_tid() < hprev->rtid()) {
            TransProxy(txn, item).add_write(nullptr);
            TXP_ACCOUNT(txp_tpcc_lock_abort1, txn.special_txp);
            return false;
        }
    }
    history_type *h = nullptr;
    if (item.has_commute()) {
        auto wval = item.template write_value<comm_type>();
        h = chain->new_history(
                Sto::commit_tid(), std::move(wval));
    } else {
        auto wval = item.template raw_write_value<TSplit*>();
        if (has_delete(item)) {
            h = chain->new_history(Sto::commit_tid(), nullptr);
            h->status_delete();
        } else {
            h = chain->new_history(Sto::commit_tid(), wval);
        }
    }
    assert(h);
    bool result = chain->template cp_lock<DBParams::Commute>(Sto::commit_tid(), h);
    if (!result && !h->status_is(MvStatus::ABORTED)) {
        chain->delete_history(h);
        TransProxy(txn, item).add_mvhistory(nullptr);
        TXP_ACCOUNT(txp_tpcc_lock_abort2, txn.special_txp);
    } else {
        TransProxy(txn, item).add_mvhistory(h);
        TXP_ACCOUNT(txp_tpcc_lock_abort3, txn.special_txp && !result);
    }
    return result;
}

template <typename K, typename V, typename DBParams>
template <typename TSplit>
bool mvcc_chain_operations<K, V, DBParams>::check_impl_per_chain(TransItem &item, Transaction &txn,
                                                                 MvObject<TSplit> *chain) {
    using history_type = typename MvObject<TSplit>::history_type;

    auto h = item.template read_value<history_type*>();
    auto result = chain->cp_check(Sto::read_tid(), h);
    TXP_ACCOUNT(txp_tpcc_check_abort2, txn.special_txp && !result);
    return result;
}

template <typename K, typename V, typename DBParams>
template <typename TSplit>
void mvcc_chain_operations<K, V, DBParams>::install_impl_per_chain(TransItem &item, Transaction &txn,
                                                                   MvObject<TSplit> *chain, void (*dcb)(void*)) {
    (void)txn;
    using history_type = typename MvObject<TSplit>::history_type;
    auto h = item.template write_value<history_type*>();
    chain->cp_install(h);
    if (dcb) {
        Transaction::rcu_call(dcb, h);
    }
}

template <typename K, typename V, typename DBParams>
template <typename TSplit>
void mvcc_chain_operations<K, V, DBParams>::cleanup_impl_per_chain(TransItem &item, bool committed,
                                                                   MvObject<TSplit> *chain) {
    (void) chain; /* XXX do not need this parameter */
    using history_type = typename MvObject<TSplit>::history_type;
    if (!committed) {
        if (item.has_mvhistory()) {
            auto h = item.template write_value<history_type*>();
            if (h) {
                h->status_txn_abort();
            }
        }
        {
            auto h = chain->find(Sto::commit_tid());
            if (h->wtid() == 0) {
                h->enqueue_for_committed();
            }
        }
    }
}

}; // namespace bench

#include "DB_uindex.hh"
#include "DB_oindex.hh"
