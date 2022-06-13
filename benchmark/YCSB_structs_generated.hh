#pragma once

#include <type_traits>
//#include "AdapterStructs.hh"

namespace ycsb_value_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct ycsb_value {
    using RecordAccessor = ycsb_value_datatypes::RecordAccessor;
    using NamedColumn = ycsb_value_datatypes::NamedColumn;

    std::array<::bench::fix_string<COL_WIDTH>, 5> odd_columns;
    std::array<::bench::fix_string<COL_WIDTH>, 5> even_columns;
};

enum class NamedColumn : int {
    odd_columns = 0,
    even_columns = 5,
    COLCOUNT = 10
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
    if constexpr (Column < NamedColumn::even_columns) {
        return NamedColumn::odd_columns;
    }
    return NamedColumn::even_columns;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::odd_columns> {
    using NamedColumn = ycsb_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = ::bench::fix_string<COL_WIDTH>;
    using value_type = std::array<::bench::fix_string<COL_WIDTH>, 5>;
    static constexpr NamedColumn Column = NamedColumn::odd_columns;
    static constexpr bool is_array = true;
};

template <>
struct accessor_info<NamedColumn::even_columns> {
    using NamedColumn = ycsb_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = ::bench::fix_string<COL_WIDTH>;
    using value_type = std::array<::bench::fix_string<COL_WIDTH>, 5>;
    static constexpr NamedColumn Column = NamedColumn::even_columns;
    static constexpr bool is_array = true;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            ycsb_value* ptr) {
        if constexpr (NamedColumn::odd_columns <= Column && Column < NamedColumn::even_columns) {
            return ptr->odd_columns[static_cast<std::underlying_type_t<NamedColumn>>(Column - NamedColumn::odd_columns)];
        }
        if constexpr (NamedColumn::even_columns <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->even_columns[static_cast<std::underlying_type_t<NamedColumn>>(Column - NamedColumn::even_columns)];
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 10;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(ycsb_value* dest, ycsb_value* src) {
        if constexpr(Cell == 0) {
            dest->odd_columns[0] = src->odd_columns[0];
            dest->odd_columns[1] = src->odd_columns[1];
            dest->odd_columns[2] = src->odd_columns[2];
            dest->odd_columns[3] = src->odd_columns[3];
            dest->odd_columns[4] = src->odd_columns[4];
            dest->even_columns[0] = src->even_columns[0];
            dest->even_columns[1] = src->even_columns[1];
            dest->even_columns[2] = src->even_columns[2];
            dest->even_columns[3] = src->even_columns[3];
            dest->even_columns[4] = src->even_columns[4];
        }
    }
};

template <>
struct SplitPolicy<1> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0, 0, 1, 1, 1, 1, 1 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 5;
        }
        if (cell == 1) {
            return 5;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(ycsb_value* dest, ycsb_value* src) {
        if constexpr(Cell == 0) {
            dest->odd_columns[0] = src->odd_columns[0];
            dest->odd_columns[1] = src->odd_columns[1];
            dest->odd_columns[2] = src->odd_columns[2];
            dest->odd_columns[3] = src->odd_columns[3];
            dest->odd_columns[4] = src->odd_columns[4];
        }
        if constexpr(Cell == 1) {
            dest->even_columns[0] = src->even_columns[0];
            dest->even_columns[1] = src->even_columns[1];
            dest->even_columns[2] = src->even_columns[2];
            dest->even_columns[3] = src->even_columns[3];
            dest->even_columns[4] = src->even_columns[4];
        }
    }
};

class RecordAccessor {
public:
    using NamedColumn = ycsb_value_datatypes::NamedColumn;
    //using SplitTable = ycsb_value_datatypes::SplitTable;
    using SplitType = ycsb_value_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<ycsb_value>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = ycsb_value;
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

    void copy_into(ycsb_value* vptr, int index=0) {
        copy_cell(index, 0, vptr, vptrs_[0]);
        copy_cell(index, 1, vptr, vptrs_[1]);
    }

    inline typename accessor_info<NamedColumn::odd_columns>::value_type& odd_columns() {
        return vptrs_[cell_of(NamedColumn::odd_columns)]->odd_columns;
    }

    inline typename accessor_info<NamedColumn::odd_columns>::type& odd_columns(
            const std::underlying_type_t<NamedColumn> index) {
        return vptrs_[cell_of(NamedColumn::odd_columns)]->odd_columns[index];
    }

    inline const typename accessor_info<NamedColumn::odd_columns>::value_type& odd_columns() const {
        return vptrs_[cell_of(NamedColumn::odd_columns)]->odd_columns;
    }

    inline const typename accessor_info<NamedColumn::odd_columns>::type& odd_columns(
            const std::underlying_type_t<NamedColumn> index) const {
        return vptrs_[cell_of(NamedColumn::odd_columns)]->odd_columns[index];
    }

    inline typename accessor_info<NamedColumn::even_columns>::value_type& even_columns() {
        return vptrs_[cell_of(NamedColumn::even_columns)]->even_columns;
    }

    inline typename accessor_info<NamedColumn::even_columns>::type& even_columns(
            const std::underlying_type_t<NamedColumn> index) {
        return vptrs_[cell_of(NamedColumn::even_columns)]->even_columns[index];
    }

    inline const typename accessor_info<NamedColumn::even_columns>::value_type& even_columns() const {
        return vptrs_[cell_of(NamedColumn::even_columns)]->even_columns;
    }

    inline const typename accessor_info<NamedColumn::even_columns>::type& even_columns(
            const std::underlying_type_t<NamedColumn> index) const {
        return vptrs_[cell_of(NamedColumn::even_columns)]->even_columns[index];
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::odd_columns <= Column && Column < NamedColumn::even_columns) {
            return vptrs_[cell_of(NamedColumn::odd_columns)]->odd_columns;
        }
        if constexpr (NamedColumn::even_columns <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::even_columns)]->even_columns;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<ycsb_value*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace ycsb_value_datatypes

using ycsb_value = ycsb_value_datatypes::ycsb_value;

