#pragma once

#include <type_traits>
//#include "AdapterStructs.hh"

namespace example_row_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct example_row {
    using RecordAccessor = example_row_datatypes::RecordAccessor;
    using NamedColumn = example_row_datatypes::NamedColumn;

    uint32_t d_ytd;
    uint32_t d_payment_cnt;
    uint32_t d_date;
    uint32_t d_tax;
    uint32_t d_next_oid;
};

enum class NamedColumn : int {
    ytd = 0,
    payment_cnt,
    date,
    tax,
    next_oid,
    COLCOUNT
};

inline constexpr NamedColumn operator+(NamedColumn nc, std::underlying_type_t<NamedColumn> index) {
    return NamedColumn(static_cast<std::underlying_type_t<NamedColumn>>(nc) + index);
}

inline constexpr NamedColumn operator+(NamedColumn nc, NamedColumn off) {
    return nc + static_cast<std::underlying_type_t<NamedColumn>>(off);
}

inline NamedColumn& operator+=(NamedColumn& nc, std::underlying_type_t<NamedColumn> index) {
    nc = static_cast<NamedColumn>(static_cast<std::underlying_type_t<NamedColumn>>(nc) + index);
    return nc;
}

inline NamedColumn& operator++(NamedColumn& nc) {
    return nc += 1;
}

inline NamedColumn& operator++(NamedColumn& nc, int) {
    return nc += 1;
}

inline constexpr NamedColumn operator-(NamedColumn nc, std::underlying_type_t<NamedColumn> index) {
    return NamedColumn(static_cast<std::underlying_type_t<NamedColumn>>(nc) - index);
}

inline constexpr NamedColumn operator-(NamedColumn nc, NamedColumn off) {
    return nc - static_cast<std::underlying_type_t<NamedColumn>>(off);
}

inline NamedColumn& operator-=(NamedColumn& nc, std::underlying_type_t<NamedColumn> index) {
    nc = static_cast<NamedColumn>(static_cast<std::underlying_type_t<NamedColumn>>(nc) - index);
    return nc;
}

inline constexpr NamedColumn operator/(NamedColumn nc, std::underlying_type_t<NamedColumn> denom) {
    return NamedColumn(static_cast<std::underlying_type_t<NamedColumn>>(nc) / denom);
}

inline std::ostream& operator<<(std::ostream& out, NamedColumn& nc) {
    out << static_cast<std::underlying_type_t<NamedColumn>>(nc);
    return out;
}

template <NamedColumn Column>
constexpr NamedColumn RoundedNamedColumn() {
    static_assert(Column < NamedColumn::COLCOUNT);
    if constexpr (Column < NamedColumn::payment_cnt) {
        return NamedColumn::ytd;
    }
    if constexpr (Column < NamedColumn::date) {
        return NamedColumn::payment_cnt;
    }
    if constexpr (Column < NamedColumn::tax) {
        return NamedColumn::date;
    }
    if constexpr (Column < NamedColumn::next_oid) {
        return NamedColumn::tax;
    }
    return NamedColumn::next_oid;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::ytd> {
    using NamedColumn = example_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::ytd;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::payment_cnt> {
    using NamedColumn = example_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::payment_cnt;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::date> {
    using NamedColumn = example_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::date;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::tax> {
    using NamedColumn = example_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::tax;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::next_oid> {
    using NamedColumn = example_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::next_oid;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            example_row* ptr) {
        if constexpr (NamedColumn::ytd <= Column && Column < NamedColumn::payment_cnt) {
            return ptr->d_ytd;
        }
        if constexpr (NamedColumn::payment_cnt <= Column && Column < NamedColumn::date) {
            return ptr->d_payment_cnt;
        }
        if constexpr (NamedColumn::date <= Column && Column < NamedColumn::tax) {
            return ptr->d_date;
        }
        if constexpr (NamedColumn::tax <= Column && Column < NamedColumn::next_oid) {
            return ptr->d_tax;
        }
        if constexpr (NamedColumn::next_oid <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->d_next_oid;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 5;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(example_row* dest, example_row* src) {
        if constexpr(Cell == 0) {
            dest->d_ytd = src->d_ytd;
            dest->d_payment_cnt = src->d_payment_cnt;
            dest->d_date = src->d_date;
            dest->d_tax = src->d_tax;
            dest->d_next_oid = src->d_next_oid;
        }
    }
};

template <>
struct SplitPolicy<1> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 1, 1, 1, 1 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 1;
        }
        if (cell == 1) {
            return 4;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(example_row* dest, example_row* src) {
        if constexpr(Cell == 0) {
            dest->d_ytd = src->d_ytd;
        }
        if constexpr(Cell == 1) {
            dest->d_payment_cnt = src->d_payment_cnt;
            dest->d_date = src->d_date;
            dest->d_tax = src->d_tax;
            dest->d_next_oid = src->d_next_oid;
        }
    }
};

class RecordAccessor {
public:
    using NamedColumn = example_row_datatypes::NamedColumn;
    //using SplitTable = example_row_datatypes::SplitTable;
    using SplitType = example_row_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<example_row>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = example_row;
    static constexpr auto DEFAULT_SPLIT = 1;
    static constexpr auto MAX_SPLITS = 2;
    static constexpr auto MAX_POINTERS = MAX_SPLITS;
    static constexpr auto POLICIES = 2;

    RecordAccessor() = default;
    template <typename... T>
    RecordAccessor(T ...vals) : vptrs_({ pointer_of(vals)... }) {}

    inline operator bool() const {
        return vptrs_[0] != nullptr;
    }

    /*
    inline ValueType* pointer_of(ValueType& value) {
        return &value;
    }
    */

    inline ValueType* pointer_of(ValueType* vptr) {
        return vptr;
    }

    template <int Index>
    inline static constexpr const auto split_of(NamedColumn column) {
        if constexpr (Index == 0) {
            return SplitPolicy<0>::column_to_cell(column);
        }
        if constexpr (Index == 1) {
            return SplitPolicy<1>::column_to_cell(column);
        }
        return 0;
    }

    inline static constexpr const auto split_of(int index, NamedColumn column) {
        if (index == 0) {
            return SplitPolicy<0>::column_to_cell(column);
        }
        if (index == 1) {
            return SplitPolicy<1>::column_to_cell(column);
        }
        return 0;
    }

    const auto cell_of(NamedColumn column) const {
        return split_of(splitindex_, column);
    }

    template <int Index>
    inline static constexpr void copy_cell(int cell, ValueType* dest, ValueType* src) {
        if constexpr (Index >= 0 && Index < POLICIES) {
            if (cell == 0) {
                SplitPolicy<Index>::template copy_cell<0>(dest, src);
                return;
            }
            if (cell == 1) {
                SplitPolicy<Index>::template copy_cell<1>(dest, src);
                return;
            }
        } else {
            (void) dest;
            (void) src;
        }
    }

    inline static constexpr void copy_cell(int index, int cell, ValueType* dest, ValueType* src) {
        if (index == 0) {
            copy_cell<0>(cell, dest, src);
            return;
        }
        if (index == 1) {
            copy_cell<1>(cell, dest, src);
            return;
        }
    }

    inline static constexpr size_t cell_col_count(int index, int cell) {
        if (index == 0) {
            return SplitPolicy<0>::cell_col_count(cell);
        }
        if (index == 1) {
            return SplitPolicy<1>::cell_col_count(cell);
        }
        return 0;
    }

    void copy_into(example_row* vptr, int index=0) {
        copy_cell(index, 0, vptr, vptrs_[0]);
        copy_cell(index, 1, vptr, vptrs_[1]);
    }

    inline typename accessor_info<NamedColumn::ytd>::value_type& d_ytd() {
        return vptrs_[cell_of(NamedColumn::ytd)]->d_ytd;
    }

    inline const typename accessor_info<NamedColumn::ytd>::value_type& d_ytd() const {
        return vptrs_[cell_of(NamedColumn::ytd)]->d_ytd;
    }

    inline typename accessor_info<NamedColumn::payment_cnt>::value_type& d_payment_cnt() {
        return vptrs_[cell_of(NamedColumn::payment_cnt)]->d_payment_cnt;
    }

    inline const typename accessor_info<NamedColumn::payment_cnt>::value_type& d_payment_cnt() const {
        return vptrs_[cell_of(NamedColumn::payment_cnt)]->d_payment_cnt;
    }

    inline typename accessor_info<NamedColumn::date>::value_type& d_date() {
        return vptrs_[cell_of(NamedColumn::date)]->d_date;
    }

    inline const typename accessor_info<NamedColumn::date>::value_type& d_date() const {
        return vptrs_[cell_of(NamedColumn::date)]->d_date;
    }

    inline typename accessor_info<NamedColumn::tax>::value_type& d_tax() {
        return vptrs_[cell_of(NamedColumn::tax)]->d_tax;
    }

    inline const typename accessor_info<NamedColumn::tax>::value_type& d_tax() const {
        return vptrs_[cell_of(NamedColumn::tax)]->d_tax;
    }

    inline typename accessor_info<NamedColumn::next_oid>::value_type& d_next_oid() {
        return vptrs_[cell_of(NamedColumn::next_oid)]->d_next_oid;
    }

    inline const typename accessor_info<NamedColumn::next_oid>::value_type& d_next_oid() const {
        return vptrs_[cell_of(NamedColumn::next_oid)]->d_next_oid;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::ytd <= Column && Column < NamedColumn::payment_cnt) {
            return vptrs_[cell_of(NamedColumn::ytd)]->d_ytd;
        }
        if constexpr (NamedColumn::payment_cnt <= Column && Column < NamedColumn::date) {
            return vptrs_[cell_of(NamedColumn::payment_cnt)]->d_payment_cnt;
        }
        if constexpr (NamedColumn::date <= Column && Column < NamedColumn::tax) {
            return vptrs_[cell_of(NamedColumn::date)]->d_date;
        }
        if constexpr (NamedColumn::tax <= Column && Column < NamedColumn::next_oid) {
            return vptrs_[cell_of(NamedColumn::tax)]->d_tax;
        }
        if constexpr (NamedColumn::next_oid <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::next_oid)]->d_next_oid;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<example_row*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace example_row_datatypes

using example_row = example_row_datatypes::example_row;

