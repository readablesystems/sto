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
template <typename RowType, typename VersImpl,
          typename RowType::RecordAccessor::SplitType DefaultSplit=
               RowType::RecordAccessor::DEFAULT_SPLIT>
class AdaptiveValueContainer {
public:
    using comm_type = commutators::Commutator<RowType>;
    using nc = typename RowType::NamedColumn;
    using RecordAccessor = typename RowType::RecordAccessor;
    using type = TransactionTid::type;
    using version_type = VersImpl;
    using ValueType = RowType;
    using SplitType = typename RowType::RecordAccessor::SplitType;
    static constexpr auto num_versions = RowType::RecordAccessor::MAX_SPLITS;
    static constexpr auto NUM_VERSIONS = RowType::RecordAccessor::MAX_SPLITS;
    static constexpr auto NUM_POINTERS = RowType::RecordAccessor::MAX_POINTERS;
    //static constexpr auto split_width = static_cast<size_t>(std::ceil(std::log2l(NUM_VERSIONS)));

    struct ColumnAccess {
        ColumnAccess(nc column, AccessType access)
            : column(column), access(access) {}

        nc column;
        AccessType access;
    };

    AdaptiveValueContainer(type v, const RowType& r) : row(r), versions_() {
        new (&versions_[0]) version_type(v);
    }
    AdaptiveValueContainer(type v, bool insert, const RowType& r) : row(r), versions_() {
        new (&versions_[0]) version_type(v, insert);
    }

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
        } else {
            return split_of(DefaultSplit, nc(col_n));
        }
    }

    std::array<AccessType, NUM_VERSIONS>
    split_accesses(const SplitType& split, std::initializer_list<ColumnAccess> accesses) {
        std::array<AccessType, NUM_VERSIONS> split_accesses = {};
        std::fill(split_accesses.begin(), split_accesses.end(), AccessType::none);

        for (auto colaccess : accesses) {
            const auto cell = split_of(split, colaccess.column);
            split_accesses[cell] |= colaccess.access;
        }

        return split_accesses;
    }

    inline RecordAccessor get() {
        return get(std::make_index_sequence<NUM_POINTERS>());
    }

    inline RecordAccessor get(const SplitType& split) {
        return get(split, std::make_index_sequence<NUM_POINTERS>());
    }

    template <size_t... Indexes>
    inline RecordAccessor get(std::index_sequence<Indexes...>) {
        return get(index_to_ptr<Indexes>()...);
    }

    template <size_t... Indexes>
    inline RecordAccessor get(const SplitType& split, std::index_sequence<Indexes...>) {
        return get(split, index_to_ptr<Indexes>()...);
    }

    template <size_t... Indexes>
    inline RecordAccessor get(const SplitType& split, RowType* ptr, std::index_sequence<Indexes...>) {
        return get(split, index_to_ptr<Indexes>(ptr)...);
    }

    template <typename... Args>
    inline RecordAccessor get(Args&&... args) {
        return get(split(), std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline RecordAccessor get(const SplitType& split, Args&&... args) {
        if constexpr (sizeof...(args) == 1 && NUM_POINTERS > 1) {
            return get(
                    split, std::forward<Args>(args)...,
                    std::make_index_sequence<NUM_POINTERS>());
        } else {
            RecordAccessor accessor(std::forward<Args>(args)...);
            accessor.splitindex_ = split;
            return accessor;
        }
    }

    template <size_t Index>
    inline RowType* index_to_ptr() {
        return &row;
    }

    template <size_t Index>
    inline RowType* index_to_ptr(RowType* ptr) {
        return ptr;
    }

    void install(const comm_type& comm) {
        comm.operate(row);
    }

    void install(RowType* new_row) {
        row = *new_row;
    }

    void install_cell(const comm_type& comm) {
        comm.operate(row);
    }

    void install_cell(int cell, RowType* const new_row) {
        install_by_cell_at_runtime(split(), cell, new_row);
        //install_by_split_cell(split(), cell, new_row);
    }

    version_type& row_version() {
        return versions_[0];
    }

    inline bool set_split(const SplitType new_split) {
        splitindex_.store(new_split, std::memory_order::memory_order_relaxed);
        //if (splitindex_ != new_split) {
        //    splitindex_ = new_split;
        //}
        return true;
    }

    inline SplitType split() {
        return splitindex_.load(std::memory_order::memory_order_relaxed);
        //return splitindex_;
    }

    inline static int split_of(SplitType split, const nc column) {
        return RecordAccessor::split_of(split, column);
    }

    version_type& version_at(int cell) {
        return versions_[cell];
    }

    RowType row;

private:
    template <nc Column=nc(0)>
    inline void install_by_cell_at_runtime(const SplitType& split, int cell, RowType* const new_row) {
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

        if constexpr (RecordAccessor::SplitTable::Splits[Split][
                static_cast<std::underlying_type_t<nc>>(Column)] == Cell) {
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

    std::atomic<SplitType> splitindex_ = {};
    std::array<version_type, num_versions> versions_;
};

/////////////////////////////
// @end Common definitions //
/////////////////////////////

// EXAMPLE:
//
// This is an example of a row layout
struct example_row {
    enum class NamedColumn : int { ytd = 0, payment_cnt, date, tax, next_oid };

    uint32_t d_ytd;
    uint32_t d_payment_cnt;
    uint32_t d_date;
    uint32_t d_tax;
    uint32_t d_next_oid;
};
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
