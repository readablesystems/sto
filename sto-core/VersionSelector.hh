#pragma once

#include <bitset>
#include <cmath>
#include "MVCC.hh"
#include "VersionBase.hh"

namespace ver_sel {

///////////////////////////////
// @begin Common definitions //
///////////////////////////////

typedef TransactionTid::type type;

// Version selector interface
template <typename T, typename VersImpl>
class VerSelBase {
public:
    typedef VersImpl version_type;

    constexpr static int map(int col_n) {
        return T::map_impl(col_n);
    }

    version_type& version_at(int cell) {
        return impl().version_at_impl(cell);
    }

    template <typename RowType>
    void install_by_cell(RowType *dst, const RowType *src, int cell) {
        return impl().install_by_cell_impl(dst, src, cell);
    }

private:
    T& impl() {
        return static_cast<T&>(*this);
    }
    const T& impl() const {
        return static_cast<const T&>(*this);
    }
};

// Default version selector has only one version for the whole row
// and always returns that version
template <typename RowType, typename VersImpl>
class VerSel : public VerSelBase<VerSel<RowType, VersImpl>, VersImpl> {
public:
    typedef VersImpl version_type;
    static constexpr size_t num_versions = 1;

    explicit VerSel(type v) : vers_(v) {}
    VerSel(type v, bool insert) : vers_(v, insert) {}

    constexpr static int map_impl(int col_n) {
        (void)col_n;
        return 0;
    }

    version_type& version_at_impl(int cell) {
        (void)cell;
        return vers_;
    }

    void install_by_cell_impl(RowType *dst, const RowType *src, int cell) {
        (void)cell;
        *dst = *src;
    }

private:
    version_type vers_;
};

}  // namespace ver_sel

// This is the actual "value type" to be put into the index
template <typename RowType, typename VersImpl>
class IndexValueContainer : ver_sel::VerSel<RowType, VersImpl> {
public:
    typedef TransactionTid::type type;
    using Selector = ver_sel::VerSel<RowType, VersImpl>;
    typedef typename Selector::version_type version_type;
    using Selector::map;
    using Selector::version_at;
    using Selector::num_versions;
    typedef commutators::Commutator<RowType> comm_type;

    IndexValueContainer(type v, const RowType& r) : Selector(v), row(r) {}
    IndexValueContainer(type v, bool insert, const RowType& r) : Selector(v, insert), row(r) {}

    RowType row;

    void install_cell(const comm_type &comm) {
        comm.operate(row);
    }

    void install_cell(int cell, const RowType *new_row) {
        Selector::install_by_cell(&row, new_row, cell);
    }

    // version_at(0) is always the row-wise version
    version_type& row_version() {
        return version_at(0);
    }
};

enum class AccessType : int8_t {none = 0, read = 1, write = 2, update = 3};

inline AccessType operator&(const AccessType& lhs, const AccessType& rhs) {
    return static_cast<AccessType>(
            static_cast<std::underlying_type_t<AccessType>>(lhs) &
            static_cast<std::underlying_type_t<AccessType>>(rhs));
};

inline AccessType operator|(const AccessType& lhs, const AccessType& rhs) {
    return static_cast<AccessType>(
            static_cast<std::underlying_type_t<AccessType>>(lhs) |
            static_cast<std::underlying_type_t<AccessType>>(rhs));
};

inline AccessType& operator|=(AccessType& lhs, const AccessType& rhs) {
    return lhs = (lhs | rhs);
};

// Container for adaptively-split value containers
template <typename ValueType, typename VersImpl,
          typename ValueType::RecordAccessor::SplitType DefaultSplit=
               ValueType::RecordAccessor::DEFAULT_SPLIT,
          bool UseMVCC=false, bool UseATS=false>
class AdaptiveValueContainer {
public:
    using comm_type = commutators::Commutator<ValueType>;
    using nc = typename ValueType::NamedColumn;
    using RecordAccessor = typename ValueType::RecordAccessor;
    using type = TransactionTid::type;
    using version_type = VersImpl;
    using access_type = AccessType;
    using SplitType = typename RecordAccessor::SplitType;
    static constexpr auto num_versions = RecordAccessor::MAX_SPLITS;
    static constexpr auto NUM_VERSIONS = RecordAccessor::MAX_SPLITS;
    static constexpr auto NUM_POINTERS = RecordAccessor::MAX_POINTERS;
    static constexpr auto NUM_POLICIES = RecordAccessor::POLICIES;
    static constexpr auto INDEX_POINTERS = std::make_index_sequence<NUM_POINTERS>{};
    //static constexpr auto split_width = static_cast<size_t>(std::ceil(std::log2l(NUM_VERSIONS)));
    using mvobject_type = MvObject<ValueType, NUM_POINTERS>;
    using mvhistory_type = typename mvobject_type::history_type;
    using RowType = std::conditional_t<UseMVCC, mvobject_type, ValueType>;

    struct ColumnAccess {
        ColumnAccess(nc column, AccessType access)
            : column(column), access(access) {}

        nc column;
        AccessType access;
    };

    //template <bool B=UseMVCC, std::enable_if_t<!B, bool> = true>
    AdaptiveValueContainer(type v, const ValueType& r) : row(r), versions_() {
        new (&versions_[0]) version_type(v);
        if constexpr (UseMVCC) {
            row.split(DefaultSplit);
        }
    }

    //template <bool B=UseMVCC, std::enable_if_t<!B, bool> = true>
    AdaptiveValueContainer(type v, bool insert, const ValueType& r) : row(r), versions_() {
        new (&versions_[0]) version_type(v, insert);
        if constexpr (UseMVCC) {
            row.head()->split(DefaultSplit);
        }
    }

    AdaptiveValueContainer(type v, bool insert) : row(), versions_() {
        new (&versions_[0]) version_type(v, insert);
        if constexpr (UseMVCC) {
            row.head()->split(DefaultSplit);
        }
    }

    /*
    template <bool B=UseMVCC, std::enable_if_t<B, bool> = true>
    AdaptiveValueContainer(type v, const ValueType& r)
        : AdaptiveValueContainer(v, r, std::make_index_sequence<NUM_POINTERS>{}) {}

    template <bool B=UseMVCC, std::enable_if_t<B, bool> = true>
    AdaptiveValueContainer(type v, bool insert, const ValueType& r)
        : AdaptiveValueContainer(v, insert, r, std::make_index_sequence<NUM_POINTERS>{}) {}

    template <bool B=UseMVCC, std::enable_if_t<B, bool> = true, size_t... I>
    AdaptiveValueContainer(
            type v, const ValueType& r, std::index_sequence<I...>)
        : row{ MvObject(*(&r + 0 * I))... }, versions_() {
        new (&versions_[0]) version_type(v);
    }

    template <bool B=UseMVCC, std::enable_if_t<B, bool> = true, size_t... I>
    AdaptiveValueContainer(
            type v, bool insert, const ValueType& r,
            std::index_sequence<I...>)
        : row{ MvObject(*(&r + 0 * I))... }, versions_() {
        new (&versions_[0]) version_type(v, insert);
    }
    */

    template <typename KeyType>
    bool access(
            std::array<AccessType, NUM_VERSIONS>& split_accesses,
            std::array<TransItem*, NUM_VERSIONS>& split_items,
            TransItem::flags_type split_bit) {
        for (size_t idx = 0; idx < split_accesses.size(); ++idx) {
            auto& access = split_accesses[idx];
            auto proxy = TransProxy(*Sto::transaction(), *split_items[idx]);
            if ((access & AccessType::read) != AccessType::none) {
                if (!proxy.observe(version_at(idx))) {
                    return false;
                }
            }
            if ((access & AccessType::write) != AccessType::none) {
                if (!proxy.acquire_write(version_at(idx))) {
                    return false;
                }
                if (proxy.item().key<KeyType>().is_row_item()) {
                    proxy.item().add_flags(split_bit);
                }
            }
        }

        return true;
    }

    constexpr static int map(int col_n) {
        if constexpr (DefaultSplit == 0) {
            (void) col_n;
            return 0;
        } else if constexpr (DefaultSplit < NUM_POLICIES) {
            return RecordAccessor::template split_of<DefaultSplit>(static_cast<nc>(col_n));
        } else {
            return RecordAccessor::template split_of<NUM_POLICIES - 1>(static_cast<nc>(col_n));
        }
    }

    template <bool B=UseMVCC>
    std::enable_if_t<B, std::tuple<
        mvhistory_type*, SplitType, std::array<AccessType, NUM_VERSIONS>>>
    mvcc_split_accesses(const type& rtid, std::initializer_list<ColumnAccess> accesses) {
        std::array<AccessType, NUM_VERSIONS> split_accesses = {};
        std::fill(split_accesses.begin(), split_accesses.end(), AccessType::none);

        auto h = row.head(0);
        mvhistory_type* ancestor = nullptr;
        while (h) {
            if (row.find_iter(h, rtid, true)) {
                break;
            }

            bool complete = true;
            std::array<mvhistory_type*, NUM_POINTERS> prevs = {};
            for (auto cell = 0; cell < NUM_POINTERS; ++cell) {
                prevs[cell] = h->prev(cell);
                if (!prevs[cell]) {
                    complete = false;
                }
            }
            if (complete) {
                ancestor = h;
            }
            h = prevs[0];
        }
        assert(h);
        auto split = h->get_split();
        auto desired_split = this->split();
        auto any_write = false;
        for (auto colaccess : accesses) {
            const auto cell = split_of(split, colaccess.column);
            split_accesses[cell] |= colaccess.access;
            if ((colaccess.access & AccessType::write) == AccessType::write) {
                any_write = true;
            }
        }
        if (split != desired_split && any_write) {
            //printf("%p Requiring resplit:", &row);
            for (auto& splitaccess : split_accesses) {
                splitaccess |= AccessType::update;
                //printf(" %d", splitaccess);
            }
            //printf("\n");
        }

        return {ancestor, split, split_accesses};
    }

    template <bool B=UseMVCC>
    //std::enable_if_t<!B, std::array<AccessType, NUM_VERSIONS>>
    std::array<AccessType, NUM_VERSIONS>
    split_accesses(const SplitType& split, std::initializer_list<ColumnAccess> accesses) {
        std::array<AccessType, NUM_VERSIONS> split_accesses = {};
        std::fill(split_accesses.begin(), split_accesses.end(), AccessType::none);

        bool any_write = false;
        for (auto colaccess : accesses) {
            const auto cell = split_of(split, colaccess.column);
            split_accesses[cell] |= colaccess.access;
            if ((colaccess.access & AccessType::write) == AccessType::write) {
                any_write = true;
            }
        }
        if (any_write && split_pending()) {
            for (auto& access : split_accesses) {
                access |= AccessType::write;
            }
        }

        return split_accesses;
    }

    inline RecordAccessor get(const SplitType& split) {
        return get(split, INDEX_POINTERS);
    }

    inline RecordAccessor get(const SplitType& split, std::array<ValueType*, NUM_POINTERS>& vptrs) {
        return get(split, vptrs, INDEX_POINTERS);
    }

    template <size_t... Indexes>
    inline RecordAccessor get(const SplitType& split, std::index_sequence<Indexes...>) {
        return get(split, index_to_ptr<Indexes>()...);
    }

    template <size_t... Indexes>
    inline RecordAccessor get(const SplitType& split, ValueType* ptr, std::index_sequence<Indexes...>) {
        return get(split, index_to_ptr<Indexes>(ptr)...);
    }

    template <size_t... Indexes>
    inline RecordAccessor get(
            const SplitType& split,
            std::array<ValueType*, NUM_POINTERS>& vptrs,
            std::index_sequence<Indexes...>) {
        return get(split, index_to_ptr<Indexes>(vptrs[Indexes])...);
    }

    template <typename... Args>
    inline RecordAccessor get(const SplitType& split, Args&&... args) {
        if constexpr (sizeof...(args) == 1 && NUM_POINTERS > 1) {
            return get(split, std::forward<Args>(args)..., INDEX_POINTERS);
        } else {
            RecordAccessor accessor(std::forward<Args>(args)...);
            accessor.splitindex_ = split;
            return accessor;
        }
    }

    /*
    template <typename... Args>
    inline RecordAccessor get(Args&&... args) {
        return get(split(), std::forward<Args>(args)...);
    }
    */

    template <size_t Index>
    inline ValueType* index_to_ptr() {
        if constexpr (UseMVCC) {
            return row.find(Sto::read_tid(), Index)->vp();
        } else {
            return &row;
        }
    }

    template <size_t Index>
    inline ValueType* index_to_ptr(ValueType* ptr) {
        return ptr;
    }

    void install(const comm_type& comm) {
        comm.operate(row);
    }

    void install(ValueType* new_row) {
        row = *new_row;
        //install_by_cell(0, new_row);
        //install_by_cell(1, new_row);
    }

    void install_cell(const comm_type& comm) {
        if constexpr (UseMVCC) {
            (void) comm;  //XXX: Implement
        } else {
            comm.operate(row);
        }
    }

    void install_cell(int cell, ValueType* const new_row) {
        if constexpr (UseMVCC) {
            (void) cell;  // XXX: implement
            (void) new_row;
        } else if constexpr (UseATS) {
            RecordAccessor::copy_cell(split(), cell, &row, new_row);
            //install_by_cell_at_runtime(split(), cell, new_row);
            //install_by_split_cell(split(), cell, new_row);
        } else {
            RecordAccessor::template copy_cell<DefaultSplit>(cell, &row, new_row);
            //install_by_cell_at_runtime(DefaultSplit, cell, new_row);
        }
    }

    version_type& row_version() {
        return versions_[0];
    }

    inline void finalize_split() {
        auto target = target_splitindex_.load(std::memory_order_relaxed);
        auto current = splitindex_.load(std::memory_order_relaxed);
        if (target != current) {
            splitindex_.compare_exchange_strong(current, target);
        }
    }

    inline void finalize_split(const SplitType new_split) {
        auto current = splitindex_.load(std::memory_order_relaxed);
        if (new_split != current) {
            splitindex_.compare_exchange_strong(current, new_split);
        }
    }

    inline bool set_split(const SplitType new_split) {
        target_splitindex_.store(new_split, std::memory_order::memory_order_relaxed);
        //if (splitindex_ != new_split) {
        //    splitindex_ = new_split;
        //}
        //fence();
        //splitindex_ = new_split;
        //fence();
        return true;
    }

    inline bool split_pending() {
        return split() != target_split();
    }

    inline SplitType split() {
        if constexpr (UseATS) {
            return splitindex_.load(std::memory_order::memory_order_relaxed);
            //return splitindex_;
        } else {
            return DefaultSplit;
        }
    }

    inline SplitType target_split() {
        if constexpr (UseATS) {
            return target_splitindex_.load(std::memory_order::memory_order_relaxed);
            //return target_splitindex_;
        } else {
            return DefaultSplit;
        }
    }

    inline static constexpr int split_of(SplitType split, const nc column) {
        return RecordAccessor::split_of(split, column);
    }

    version_type& version_at(int cell) {
        return versions_[cell];
    }

    RowType row;

private:
    template <nc Column=nc(0)>
    inline void install_by_cell_at_runtime(const SplitType& split, int cell, ValueType* const new_row) {
        static_assert(Column < nc::COLCOUNT);

        if (split_of(split, Column) == cell) {
            auto& old_col = RecordAccessor::template get_value<Column>(&row);
            auto& new_col = RecordAccessor::template get_value<Column>(new_row);
            old_col = new_col;
        }

        // This constexpr is necessary for template deduction purposes
        if constexpr (Column + 1 < nc::COLCOUNT) {
            install_by_cell_at_runtime<Column + 1>(split, cell, new_row);
        }
    }

    /*
    template <SplitType Split=0>
    inline void install_by_split_cell(const SplitType& split, int cell, RowType* const new_row) {
        static_assert(Split >= 0);
        static_assert(Split < RecordAccessor::SplitTable::Size);

        if (Split != split) {
            if constexpr (Split + 1 < RecordAccessor::SplitTable::Size) {
                install_by_split_cell<Split + 1>(split, cell, new_row);
            }
            return;
        }

        install_by_cell<Split>(cell, new_row);
    }

    template <SplitType Split, int Cell=0>
    inline void install_by_cell(int cell, RowType* const new_row) {
        static_assert(Cell >= 0);
        static_assert(Cell < RecordAccessor::MAX_SPLITS);

        if (Cell != cell) {
            if constexpr (Cell + 1 < RecordAccessor::MAX_SPLITS) {
                install_by_cell<Split, Cell + 1>(cell, new_row);
            }
            return;
        }

        install_by_column<Split, Cell>(new_row);
    }

    template <SplitType Split, int Cell, nc Column=nc(0)>
    inline void install_by_column(RowType* const new_row) {
        static_assert(Column >= nc(0));
        static_assert(Column < nc::COLCOUNT);

        if constexpr (split_of(Split, Column) == Cell) {
            auto& old_col = RecordAccessor::template get_value<Column>(&row);
            auto& new_col = RecordAccessor::template get_value<Column>(new_row);
            old_col = new_col;
        }

        // This constexpr is necessary for template deduction purposes
        if constexpr (Column + 1 < nc::COLCOUNT) {
            install_by_column<Split, Cell, Column + 1>(new_row);
        } else {
            (void) new_row;
        }

    }
    */

    std::atomic<SplitType> splitindex_ = {DefaultSplit};
    std::atomic<SplitType> target_splitindex_ = {DefaultSplit};
    //SplitType splitindex_ = {};
    std::array<version_type, num_versions> versions_;
};

/////////////////////////////
// @end Common definitions //
/////////////////////////////

// EXAMPLE:
//
// This is an example of a row layout
/*
struct example_row {
    enum class NamedColumn : int { ytd = 0, payment_cnt, date, tax, next_oid };

    uint32_t d_ytd;
    uint32_t d_payment_cnt;
    uint32_t d_date;
    uint32_t d_tax;
    uint32_t d_next_oid;
};
*/
#include "example_row_generated.hh"
// The programer can annotate something like this:
//
// @@@DefineSelector
// class example_row:
//   d_ytd, d_payment_cnt, d_date, d_tax, d_next_oid
// group:
//   {d_ytd}, {d_payment_cnt, d_date, d_tax, d_next_oid}
// @@@SelectorDefined


namespace ver_sel {

// This is the version selector class that should be auto-generated
template <typename VersImpl>
class VerSel<example_row, VersImpl> : public VerSelBase<VerSel<example_row, VersImpl>, VersImpl> {
public:
    typedef VersImpl version_type;
    static constexpr size_t num_versions = 2;

    explicit VerSel(type v) : vers_() {
        new (&vers_[0]) version_type(v);
    }
    VerSel(type v, bool insert) : vers_() {
        new (&vers_[0]) version_type(v, insert);
    }

    constexpr static int map_impl(int col_n) {
        if (col_n == 0)
            return 0;
        else
            return 1;
    }

    version_type& version_at_impl(int cell) {
        return vers_[cell];
    }

    void install_by_cell_impl(example_row *dst, const example_row *src, int cell) {
        if (cell == 0) {
            dst->d_ytd = src->d_ytd;
        } else {
            dst->d_payment_cnt = src->d_payment_cnt;
            dst->d_date = src->d_date;
            dst->d_tax = src->d_tax;
            dst->d_next_oid = src->d_next_oid;
        }
    }

private:
    version_type vers_[num_versions];
};

}  // namespace ver_sel

// And in our DB index we can simply use the following type as value type:
// (assuming we use bench::ordered_index<K, V, DBParams>)
// IndexValueContainer<V, version_type>;
