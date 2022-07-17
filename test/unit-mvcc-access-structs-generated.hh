#pragma once

#include <type_traits>
//#include "AdapterStructs.hh"

namespace index_value_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct index_value {
    using RecordAccessor = index_value_datatypes::RecordAccessor;
    using NamedColumn = index_value_datatypes::NamedColumn;

    int64_t value_1;
    int64_t value_2a;
    int64_t value_2b;
};

enum class NamedColumn : int {
    value_1 = 0,
    value_2a,
    value_2b,
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
    if constexpr (Column < NamedColumn::value_2a) {
        return NamedColumn::value_1;
    }
    if constexpr (Column < NamedColumn::value_2b) {
        return NamedColumn::value_2a;
    }
    return NamedColumn::value_2b;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::value_1> {
    using NamedColumn = index_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int64_t;
    using value_type = int64_t;
    static constexpr NamedColumn Column = NamedColumn::value_1;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::value_2a> {
    using NamedColumn = index_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int64_t;
    using value_type = int64_t;
    static constexpr NamedColumn Column = NamedColumn::value_2a;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::value_2b> {
    using NamedColumn = index_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int64_t;
    using value_type = int64_t;
    static constexpr NamedColumn Column = NamedColumn::value_2b;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            index_value* ptr) {
        if constexpr (NamedColumn::value_1 <= Column && Column < NamedColumn::value_2a) {
            return ptr->value_1;
        }
        if constexpr (NamedColumn::value_2a <= Column && Column < NamedColumn::value_2b) {
            return ptr->value_2a;
        }
        if constexpr (NamedColumn::value_2b <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->value_2b;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 3;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(index_value* dest, index_value* src) {
        if constexpr(Cell == 0) {
            *dest = *src;
        }
        (void) dest; (void) src;
    }
};

template <>
struct SplitPolicy<1> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 1, 1 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 1;
        }
        if (cell == 1) {
            return 2;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(index_value* dest, index_value* src) {
        if constexpr(Cell == 0) {
            dest->value_1 = src->value_1;
        }
        if constexpr(Cell == 1) {
            dest->value_2a = src->value_2a;
            dest->value_2b = src->value_2b;
        }
        (void) dest; (void) src;
    }
};

class RecordAccessor {
public:
    using NamedColumn = index_value_datatypes::NamedColumn;
    //using SplitTable = index_value_datatypes::SplitTable;
    using SplitType = index_value_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<index_value>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = index_value;
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

    template <int Cell>
    inline static constexpr void copy_split_cell(int index, ValueType* dest, ValueType* src) {
        if (index == 0) {
            SplitPolicy<0>::copy_cell<Cell>(dest, src);
            return;
        }
        if (index == 1) {
            SplitPolicy<1>::copy_cell<Cell>(dest, src);
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

    inline void copy_into(index_value* vptr, int index) {
        if (vptrs_[0]) {
            copy_split_cell<0>(index, vptr, vptrs_[0]);
        }
        if (vptrs_[1]) {
            copy_split_cell<1>(index, vptr, vptrs_[1]);
        }
    }

    inline void copy_into(index_value* vptr) {
        copy_into(vptr, splitindex_);
    }
    inline typename accessor_info<NamedColumn::value_1>::value_type& value_1() {
        return vptrs_[cell_of(NamedColumn::value_1)]->value_1;
    }

    inline const typename accessor_info<NamedColumn::value_1>::value_type& value_1() const {
        return vptrs_[cell_of(NamedColumn::value_1)]->value_1;
    }

    inline typename accessor_info<NamedColumn::value_2a>::value_type& value_2a() {
        return vptrs_[cell_of(NamedColumn::value_2a)]->value_2a;
    }

    inline const typename accessor_info<NamedColumn::value_2a>::value_type& value_2a() const {
        return vptrs_[cell_of(NamedColumn::value_2a)]->value_2a;
    }

    inline typename accessor_info<NamedColumn::value_2b>::value_type& value_2b() {
        return vptrs_[cell_of(NamedColumn::value_2b)]->value_2b;
    }

    inline const typename accessor_info<NamedColumn::value_2b>::value_type& value_2b() const {
        return vptrs_[cell_of(NamedColumn::value_2b)]->value_2b;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::value_1 <= Column && Column < NamedColumn::value_2a) {
            return vptrs_[cell_of(NamedColumn::value_1)]->value_1;
        }
        if constexpr (NamedColumn::value_2a <= Column && Column < NamedColumn::value_2b) {
            return vptrs_[cell_of(NamedColumn::value_2a)]->value_2a;
        }
        if constexpr (NamedColumn::value_2b <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::value_2b)]->value_2b;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<index_value*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace index_value_datatypes

using index_value = index_value_datatypes::index_value;

