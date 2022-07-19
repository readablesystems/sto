#pragma once

#include <type_traits>
//#include "AdapterStructs.hh"

namespace warehouse_value_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct warehouse_value {
    using RecordAccessor = warehouse_value_datatypes::RecordAccessor;
    using NamedColumn = warehouse_value_datatypes::NamedColumn;

    var_string<10> w_name;
    var_string<20> w_street_1;
    var_string<20> w_street_2;
    var_string<20> w_city;
    fix_string<2> w_state;
    fix_string<9> w_zip;
    int64_t w_tax;
    uint64_t w_ytd;
};

enum class NamedColumn : int {
    w_name = 0,
    w_street_1,
    w_street_2,
    w_city,
    w_state,
    w_zip,
    w_tax,
    w_ytd,
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
    if constexpr (Column < NamedColumn::w_street_1) {
        return NamedColumn::w_name;
    }
    if constexpr (Column < NamedColumn::w_street_2) {
        return NamedColumn::w_street_1;
    }
    if constexpr (Column < NamedColumn::w_city) {
        return NamedColumn::w_street_2;
    }
    if constexpr (Column < NamedColumn::w_state) {
        return NamedColumn::w_city;
    }
    if constexpr (Column < NamedColumn::w_zip) {
        return NamedColumn::w_state;
    }
    if constexpr (Column < NamedColumn::w_tax) {
        return NamedColumn::w_zip;
    }
    if constexpr (Column < NamedColumn::w_ytd) {
        return NamedColumn::w_tax;
    }
    return NamedColumn::w_ytd;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::w_name> {
    using NamedColumn = warehouse_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<10>;
    using value_type = var_string<10>;
    static constexpr NamedColumn Column = NamedColumn::w_name;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::w_street_1> {
    using NamedColumn = warehouse_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr NamedColumn Column = NamedColumn::w_street_1;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::w_street_2> {
    using NamedColumn = warehouse_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr NamedColumn Column = NamedColumn::w_street_2;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::w_city> {
    using NamedColumn = warehouse_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr NamedColumn Column = NamedColumn::w_city;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::w_state> {
    using NamedColumn = warehouse_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = fix_string<2>;
    using value_type = fix_string<2>;
    static constexpr NamedColumn Column = NamedColumn::w_state;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::w_zip> {
    using NamedColumn = warehouse_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = fix_string<9>;
    using value_type = fix_string<9>;
    static constexpr NamedColumn Column = NamedColumn::w_zip;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::w_tax> {
    using NamedColumn = warehouse_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int64_t;
    using value_type = int64_t;
    static constexpr NamedColumn Column = NamedColumn::w_tax;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::w_ytd> {
    using NamedColumn = warehouse_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::w_ytd;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            warehouse_value* ptr) {
        if constexpr (NamedColumn::w_name <= Column && Column < NamedColumn::w_street_1) {
            return ptr->w_name;
        }
        if constexpr (NamedColumn::w_street_1 <= Column && Column < NamedColumn::w_street_2) {
            return ptr->w_street_1;
        }
        if constexpr (NamedColumn::w_street_2 <= Column && Column < NamedColumn::w_city) {
            return ptr->w_street_2;
        }
        if constexpr (NamedColumn::w_city <= Column && Column < NamedColumn::w_state) {
            return ptr->w_city;
        }
        if constexpr (NamedColumn::w_state <= Column && Column < NamedColumn::w_zip) {
            return ptr->w_state;
        }
        if constexpr (NamedColumn::w_zip <= Column && Column < NamedColumn::w_tax) {
            return ptr->w_zip;
        }
        if constexpr (NamedColumn::w_tax <= Column && Column < NamedColumn::w_ytd) {
            return ptr->w_tax;
        }
        if constexpr (NamedColumn::w_ytd <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->w_ytd;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 8;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(warehouse_value* dest, warehouse_value* src) {
        if constexpr(Cell == 0) {
            *dest = *src;
        }
        (void) dest; (void) src;
    }
};

template <>
struct SplitPolicy<1> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0, 0, 0, 0, 1 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 7;
        }
        if (cell == 1) {
            return 1;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(warehouse_value* dest, warehouse_value* src) {
        if constexpr(Cell == 0) {
            dest->w_name = src->w_name;
            dest->w_street_1 = src->w_street_1;
            dest->w_street_2 = src->w_street_2;
            dest->w_city = src->w_city;
            dest->w_state = src->w_state;
            dest->w_zip = src->w_zip;
            dest->w_tax = src->w_tax;
        }
        if constexpr(Cell == 1) {
            dest->w_ytd = src->w_ytd;
        }
        (void) dest; (void) src;
    }
};

class RecordAccessor {
public:
    using NamedColumn = warehouse_value_datatypes::NamedColumn;
    //using SplitTable = warehouse_value_datatypes::SplitTable;
    using SplitType = warehouse_value_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<warehouse_value>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = warehouse_value;
    static constexpr auto DEFAULT_SPLIT = 0;
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

    inline void copy_into(warehouse_value* vptr, int index) {
        if (vptrs_[0]) {
            copy_split_cell<0>(index, vptr, vptrs_[0]);
        }
        if (vptrs_[1]) {
            copy_split_cell<1>(index, vptr, vptrs_[1]);
        }
    }

    inline void copy_into(warehouse_value* vptr) {
        copy_into(vptr, splitindex_);
    }
    inline typename accessor_info<NamedColumn::w_name>::value_type& w_name() {
        return vptrs_[cell_of(NamedColumn::w_name)]->w_name;
    }

    inline const typename accessor_info<NamedColumn::w_name>::value_type& w_name() const {
        return vptrs_[cell_of(NamedColumn::w_name)]->w_name;
    }

    inline typename accessor_info<NamedColumn::w_street_1>::value_type& w_street_1() {
        return vptrs_[cell_of(NamedColumn::w_street_1)]->w_street_1;
    }

    inline const typename accessor_info<NamedColumn::w_street_1>::value_type& w_street_1() const {
        return vptrs_[cell_of(NamedColumn::w_street_1)]->w_street_1;
    }

    inline typename accessor_info<NamedColumn::w_street_2>::value_type& w_street_2() {
        return vptrs_[cell_of(NamedColumn::w_street_2)]->w_street_2;
    }

    inline const typename accessor_info<NamedColumn::w_street_2>::value_type& w_street_2() const {
        return vptrs_[cell_of(NamedColumn::w_street_2)]->w_street_2;
    }

    inline typename accessor_info<NamedColumn::w_city>::value_type& w_city() {
        return vptrs_[cell_of(NamedColumn::w_city)]->w_city;
    }

    inline const typename accessor_info<NamedColumn::w_city>::value_type& w_city() const {
        return vptrs_[cell_of(NamedColumn::w_city)]->w_city;
    }

    inline typename accessor_info<NamedColumn::w_state>::value_type& w_state() {
        return vptrs_[cell_of(NamedColumn::w_state)]->w_state;
    }

    inline const typename accessor_info<NamedColumn::w_state>::value_type& w_state() const {
        return vptrs_[cell_of(NamedColumn::w_state)]->w_state;
    }

    inline typename accessor_info<NamedColumn::w_zip>::value_type& w_zip() {
        return vptrs_[cell_of(NamedColumn::w_zip)]->w_zip;
    }

    inline const typename accessor_info<NamedColumn::w_zip>::value_type& w_zip() const {
        return vptrs_[cell_of(NamedColumn::w_zip)]->w_zip;
    }

    inline typename accessor_info<NamedColumn::w_tax>::value_type& w_tax() {
        return vptrs_[cell_of(NamedColumn::w_tax)]->w_tax;
    }

    inline const typename accessor_info<NamedColumn::w_tax>::value_type& w_tax() const {
        return vptrs_[cell_of(NamedColumn::w_tax)]->w_tax;
    }

    inline typename accessor_info<NamedColumn::w_ytd>::value_type& w_ytd() {
        return vptrs_[cell_of(NamedColumn::w_ytd)]->w_ytd;
    }

    inline const typename accessor_info<NamedColumn::w_ytd>::value_type& w_ytd() const {
        return vptrs_[cell_of(NamedColumn::w_ytd)]->w_ytd;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::w_name <= Column && Column < NamedColumn::w_street_1) {
            return vptrs_[cell_of(NamedColumn::w_name)]->w_name;
        }
        if constexpr (NamedColumn::w_street_1 <= Column && Column < NamedColumn::w_street_2) {
            return vptrs_[cell_of(NamedColumn::w_street_1)]->w_street_1;
        }
        if constexpr (NamedColumn::w_street_2 <= Column && Column < NamedColumn::w_city) {
            return vptrs_[cell_of(NamedColumn::w_street_2)]->w_street_2;
        }
        if constexpr (NamedColumn::w_city <= Column && Column < NamedColumn::w_state) {
            return vptrs_[cell_of(NamedColumn::w_city)]->w_city;
        }
        if constexpr (NamedColumn::w_state <= Column && Column < NamedColumn::w_zip) {
            return vptrs_[cell_of(NamedColumn::w_state)]->w_state;
        }
        if constexpr (NamedColumn::w_zip <= Column && Column < NamedColumn::w_tax) {
            return vptrs_[cell_of(NamedColumn::w_zip)]->w_zip;
        }
        if constexpr (NamedColumn::w_tax <= Column && Column < NamedColumn::w_ytd) {
            return vptrs_[cell_of(NamedColumn::w_tax)]->w_tax;
        }
        if constexpr (NamedColumn::w_ytd <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::w_ytd)]->w_ytd;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<warehouse_value*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace warehouse_value_datatypes

using warehouse_value = warehouse_value_datatypes::warehouse_value;

namespace district_value_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct district_value {
    using RecordAccessor = district_value_datatypes::RecordAccessor;
    using NamedColumn = district_value_datatypes::NamedColumn;

    var_string<10> d_name;
    var_string<20> d_street_1;
    var_string<20> d_street_2;
    var_string<20> d_city;
    fix_string<2> d_state;
    fix_string<9> d_zip;
    int64_t d_tax;
    int64_t d_ytd;
};

enum class NamedColumn : int {
    d_name = 0,
    d_street_1,
    d_street_2,
    d_city,
    d_state,
    d_zip,
    d_tax,
    d_ytd,
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
    if constexpr (Column < NamedColumn::d_street_1) {
        return NamedColumn::d_name;
    }
    if constexpr (Column < NamedColumn::d_street_2) {
        return NamedColumn::d_street_1;
    }
    if constexpr (Column < NamedColumn::d_city) {
        return NamedColumn::d_street_2;
    }
    if constexpr (Column < NamedColumn::d_state) {
        return NamedColumn::d_city;
    }
    if constexpr (Column < NamedColumn::d_zip) {
        return NamedColumn::d_state;
    }
    if constexpr (Column < NamedColumn::d_tax) {
        return NamedColumn::d_zip;
    }
    if constexpr (Column < NamedColumn::d_ytd) {
        return NamedColumn::d_tax;
    }
    return NamedColumn::d_ytd;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::d_name> {
    using NamedColumn = district_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<10>;
    using value_type = var_string<10>;
    static constexpr NamedColumn Column = NamedColumn::d_name;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::d_street_1> {
    using NamedColumn = district_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr NamedColumn Column = NamedColumn::d_street_1;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::d_street_2> {
    using NamedColumn = district_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr NamedColumn Column = NamedColumn::d_street_2;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::d_city> {
    using NamedColumn = district_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr NamedColumn Column = NamedColumn::d_city;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::d_state> {
    using NamedColumn = district_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = fix_string<2>;
    using value_type = fix_string<2>;
    static constexpr NamedColumn Column = NamedColumn::d_state;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::d_zip> {
    using NamedColumn = district_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = fix_string<9>;
    using value_type = fix_string<9>;
    static constexpr NamedColumn Column = NamedColumn::d_zip;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::d_tax> {
    using NamedColumn = district_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int64_t;
    using value_type = int64_t;
    static constexpr NamedColumn Column = NamedColumn::d_tax;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::d_ytd> {
    using NamedColumn = district_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int64_t;
    using value_type = int64_t;
    static constexpr NamedColumn Column = NamedColumn::d_ytd;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            district_value* ptr) {
        if constexpr (NamedColumn::d_name <= Column && Column < NamedColumn::d_street_1) {
            return ptr->d_name;
        }
        if constexpr (NamedColumn::d_street_1 <= Column && Column < NamedColumn::d_street_2) {
            return ptr->d_street_1;
        }
        if constexpr (NamedColumn::d_street_2 <= Column && Column < NamedColumn::d_city) {
            return ptr->d_street_2;
        }
        if constexpr (NamedColumn::d_city <= Column && Column < NamedColumn::d_state) {
            return ptr->d_city;
        }
        if constexpr (NamedColumn::d_state <= Column && Column < NamedColumn::d_zip) {
            return ptr->d_state;
        }
        if constexpr (NamedColumn::d_zip <= Column && Column < NamedColumn::d_tax) {
            return ptr->d_zip;
        }
        if constexpr (NamedColumn::d_tax <= Column && Column < NamedColumn::d_ytd) {
            return ptr->d_tax;
        }
        if constexpr (NamedColumn::d_ytd <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->d_ytd;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 8;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(district_value* dest, district_value* src) {
        if constexpr(Cell == 0) {
            *dest = *src;
        }
        (void) dest; (void) src;
    }
};

template <>
struct SplitPolicy<1> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0, 0, 0, 0, 1 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 7;
        }
        if (cell == 1) {
            return 1;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(district_value* dest, district_value* src) {
        if constexpr(Cell == 0) {
            dest->d_name = src->d_name;
            dest->d_street_1 = src->d_street_1;
            dest->d_street_2 = src->d_street_2;
            dest->d_city = src->d_city;
            dest->d_state = src->d_state;
            dest->d_zip = src->d_zip;
            dest->d_tax = src->d_tax;
        }
        if constexpr(Cell == 1) {
            dest->d_ytd = src->d_ytd;
        }
        (void) dest; (void) src;
    }
};

class RecordAccessor {
public:
    using NamedColumn = district_value_datatypes::NamedColumn;
    //using SplitTable = district_value_datatypes::SplitTable;
    using SplitType = district_value_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<district_value>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = district_value;
    static constexpr auto DEFAULT_SPLIT = 0;
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

    inline void copy_into(district_value* vptr, int index) {
        if (vptrs_[0]) {
            copy_split_cell<0>(index, vptr, vptrs_[0]);
        }
        if (vptrs_[1]) {
            copy_split_cell<1>(index, vptr, vptrs_[1]);
        }
    }

    inline void copy_into(district_value* vptr) {
        copy_into(vptr, splitindex_);
    }
    inline typename accessor_info<NamedColumn::d_name>::value_type& d_name() {
        return vptrs_[cell_of(NamedColumn::d_name)]->d_name;
    }

    inline const typename accessor_info<NamedColumn::d_name>::value_type& d_name() const {
        return vptrs_[cell_of(NamedColumn::d_name)]->d_name;
    }

    inline typename accessor_info<NamedColumn::d_street_1>::value_type& d_street_1() {
        return vptrs_[cell_of(NamedColumn::d_street_1)]->d_street_1;
    }

    inline const typename accessor_info<NamedColumn::d_street_1>::value_type& d_street_1() const {
        return vptrs_[cell_of(NamedColumn::d_street_1)]->d_street_1;
    }

    inline typename accessor_info<NamedColumn::d_street_2>::value_type& d_street_2() {
        return vptrs_[cell_of(NamedColumn::d_street_2)]->d_street_2;
    }

    inline const typename accessor_info<NamedColumn::d_street_2>::value_type& d_street_2() const {
        return vptrs_[cell_of(NamedColumn::d_street_2)]->d_street_2;
    }

    inline typename accessor_info<NamedColumn::d_city>::value_type& d_city() {
        return vptrs_[cell_of(NamedColumn::d_city)]->d_city;
    }

    inline const typename accessor_info<NamedColumn::d_city>::value_type& d_city() const {
        return vptrs_[cell_of(NamedColumn::d_city)]->d_city;
    }

    inline typename accessor_info<NamedColumn::d_state>::value_type& d_state() {
        return vptrs_[cell_of(NamedColumn::d_state)]->d_state;
    }

    inline const typename accessor_info<NamedColumn::d_state>::value_type& d_state() const {
        return vptrs_[cell_of(NamedColumn::d_state)]->d_state;
    }

    inline typename accessor_info<NamedColumn::d_zip>::value_type& d_zip() {
        return vptrs_[cell_of(NamedColumn::d_zip)]->d_zip;
    }

    inline const typename accessor_info<NamedColumn::d_zip>::value_type& d_zip() const {
        return vptrs_[cell_of(NamedColumn::d_zip)]->d_zip;
    }

    inline typename accessor_info<NamedColumn::d_tax>::value_type& d_tax() {
        return vptrs_[cell_of(NamedColumn::d_tax)]->d_tax;
    }

    inline const typename accessor_info<NamedColumn::d_tax>::value_type& d_tax() const {
        return vptrs_[cell_of(NamedColumn::d_tax)]->d_tax;
    }

    inline typename accessor_info<NamedColumn::d_ytd>::value_type& d_ytd() {
        return vptrs_[cell_of(NamedColumn::d_ytd)]->d_ytd;
    }

    inline const typename accessor_info<NamedColumn::d_ytd>::value_type& d_ytd() const {
        return vptrs_[cell_of(NamedColumn::d_ytd)]->d_ytd;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::d_name <= Column && Column < NamedColumn::d_street_1) {
            return vptrs_[cell_of(NamedColumn::d_name)]->d_name;
        }
        if constexpr (NamedColumn::d_street_1 <= Column && Column < NamedColumn::d_street_2) {
            return vptrs_[cell_of(NamedColumn::d_street_1)]->d_street_1;
        }
        if constexpr (NamedColumn::d_street_2 <= Column && Column < NamedColumn::d_city) {
            return vptrs_[cell_of(NamedColumn::d_street_2)]->d_street_2;
        }
        if constexpr (NamedColumn::d_city <= Column && Column < NamedColumn::d_state) {
            return vptrs_[cell_of(NamedColumn::d_city)]->d_city;
        }
        if constexpr (NamedColumn::d_state <= Column && Column < NamedColumn::d_zip) {
            return vptrs_[cell_of(NamedColumn::d_state)]->d_state;
        }
        if constexpr (NamedColumn::d_zip <= Column && Column < NamedColumn::d_tax) {
            return vptrs_[cell_of(NamedColumn::d_zip)]->d_zip;
        }
        if constexpr (NamedColumn::d_tax <= Column && Column < NamedColumn::d_ytd) {
            return vptrs_[cell_of(NamedColumn::d_tax)]->d_tax;
        }
        if constexpr (NamedColumn::d_ytd <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::d_ytd)]->d_ytd;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<district_value*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace district_value_datatypes

using district_value = district_value_datatypes::district_value;

namespace customer_idx_value_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct customer_idx_value {
    using RecordAccessor = customer_idx_value_datatypes::RecordAccessor;
    using NamedColumn = customer_idx_value_datatypes::NamedColumn;

    std::list<uint64_t> c_ids;
};

enum class NamedColumn : int {
    c_ids = 0,
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
    return NamedColumn::c_ids;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::c_ids> {
    using NamedColumn = customer_idx_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = std::list<uint64_t>;
    using value_type = std::list<uint64_t>;
    static constexpr NamedColumn Column = NamedColumn::c_ids;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            customer_idx_value* ptr) {
        if constexpr (NamedColumn::c_ids <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->c_ids;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 1;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(customer_idx_value* dest, customer_idx_value* src) {
        if constexpr(Cell == 0) {
            *dest = *src;
        }
        (void) dest; (void) src;
    }
};

class RecordAccessor {
public:
    using NamedColumn = customer_idx_value_datatypes::NamedColumn;
    //using SplitTable = customer_idx_value_datatypes::SplitTable;
    using SplitType = customer_idx_value_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<customer_idx_value>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = customer_idx_value;
    static constexpr auto DEFAULT_SPLIT = 0;
    static constexpr auto MAX_SPLITS = 2;
    static constexpr auto MAX_POINTERS = MAX_SPLITS;
    static constexpr auto POLICIES = 1;

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
        return 0;
    }

    inline static constexpr const auto split_of(int index, NamedColumn column) {
        if (index == 0) {
            return SplitPolicy<0>::column_to_cell(column);
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
    }

    template <int Cell>
    inline static constexpr void copy_split_cell(int index, ValueType* dest, ValueType* src) {
        if (index == 0) {
            SplitPolicy<0>::copy_cell<Cell>(dest, src);
            return;
        }
    }

    inline static constexpr size_t cell_col_count(int index, int cell) {
        if (index == 0) {
            return SplitPolicy<0>::cell_col_count(cell);
        }
        return 0;
    }

    inline void copy_into(customer_idx_value* vptr, int index) {
        if (vptrs_[0]) {
            copy_split_cell<0>(index, vptr, vptrs_[0]);
        }
    }

    inline void copy_into(customer_idx_value* vptr) {
        copy_into(vptr, splitindex_);
    }
    inline typename accessor_info<NamedColumn::c_ids>::value_type& c_ids() {
        return vptrs_[cell_of(NamedColumn::c_ids)]->c_ids;
    }

    inline const typename accessor_info<NamedColumn::c_ids>::value_type& c_ids() const {
        return vptrs_[cell_of(NamedColumn::c_ids)]->c_ids;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::c_ids <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::c_ids)]->c_ids;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<customer_idx_value*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace customer_idx_value_datatypes

using customer_idx_value = customer_idx_value_datatypes::customer_idx_value;

namespace customer_value_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct customer_value {
    using RecordAccessor = customer_value_datatypes::RecordAccessor;
    using NamedColumn = customer_value_datatypes::NamedColumn;

    var_string<16> c_first;
    fix_string<2> c_middle;
    var_string<16> c_last;
    var_string<20> c_street_1;
    var_string<20> c_street_2;
    var_string<20> c_city;
    fix_string<2> c_state;
    fix_string<9> c_zip;
    fix_string<16> c_phone;
    uint32_t c_since;
    fix_string<2> c_credit;
    int64_t c_credit_lim;
    int64_t c_discount;
    int64_t c_balance;
    int64_t c_ytd_payment;
    uint16_t c_payment_cnt;
    uint16_t c_delivery_cnt;
    fix_string<500> c_data;
};

enum class NamedColumn : int {
    c_first = 0,
    c_middle,
    c_last,
    c_street_1,
    c_street_2,
    c_city,
    c_state,
    c_zip,
    c_phone,
    c_since,
    c_credit,
    c_credit_lim,
    c_discount,
    c_balance,
    c_ytd_payment,
    c_payment_cnt,
    c_delivery_cnt,
    c_data,
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
    if constexpr (Column < NamedColumn::c_middle) {
        return NamedColumn::c_first;
    }
    if constexpr (Column < NamedColumn::c_last) {
        return NamedColumn::c_middle;
    }
    if constexpr (Column < NamedColumn::c_street_1) {
        return NamedColumn::c_last;
    }
    if constexpr (Column < NamedColumn::c_street_2) {
        return NamedColumn::c_street_1;
    }
    if constexpr (Column < NamedColumn::c_city) {
        return NamedColumn::c_street_2;
    }
    if constexpr (Column < NamedColumn::c_state) {
        return NamedColumn::c_city;
    }
    if constexpr (Column < NamedColumn::c_zip) {
        return NamedColumn::c_state;
    }
    if constexpr (Column < NamedColumn::c_phone) {
        return NamedColumn::c_zip;
    }
    if constexpr (Column < NamedColumn::c_since) {
        return NamedColumn::c_phone;
    }
    if constexpr (Column < NamedColumn::c_credit) {
        return NamedColumn::c_since;
    }
    if constexpr (Column < NamedColumn::c_credit_lim) {
        return NamedColumn::c_credit;
    }
    if constexpr (Column < NamedColumn::c_discount) {
        return NamedColumn::c_credit_lim;
    }
    if constexpr (Column < NamedColumn::c_balance) {
        return NamedColumn::c_discount;
    }
    if constexpr (Column < NamedColumn::c_ytd_payment) {
        return NamedColumn::c_balance;
    }
    if constexpr (Column < NamedColumn::c_payment_cnt) {
        return NamedColumn::c_ytd_payment;
    }
    if constexpr (Column < NamedColumn::c_delivery_cnt) {
        return NamedColumn::c_payment_cnt;
    }
    if constexpr (Column < NamedColumn::c_data) {
        return NamedColumn::c_delivery_cnt;
    }
    return NamedColumn::c_data;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::c_first> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<16>;
    using value_type = var_string<16>;
    static constexpr NamedColumn Column = NamedColumn::c_first;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_middle> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = fix_string<2>;
    using value_type = fix_string<2>;
    static constexpr NamedColumn Column = NamedColumn::c_middle;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_last> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<16>;
    using value_type = var_string<16>;
    static constexpr NamedColumn Column = NamedColumn::c_last;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_street_1> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr NamedColumn Column = NamedColumn::c_street_1;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_street_2> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr NamedColumn Column = NamedColumn::c_street_2;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_city> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr NamedColumn Column = NamedColumn::c_city;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_state> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = fix_string<2>;
    using value_type = fix_string<2>;
    static constexpr NamedColumn Column = NamedColumn::c_state;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_zip> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = fix_string<9>;
    using value_type = fix_string<9>;
    static constexpr NamedColumn Column = NamedColumn::c_zip;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_phone> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = fix_string<16>;
    using value_type = fix_string<16>;
    static constexpr NamedColumn Column = NamedColumn::c_phone;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_since> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::c_since;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_credit> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = fix_string<2>;
    using value_type = fix_string<2>;
    static constexpr NamedColumn Column = NamedColumn::c_credit;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_credit_lim> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int64_t;
    using value_type = int64_t;
    static constexpr NamedColumn Column = NamedColumn::c_credit_lim;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_discount> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int64_t;
    using value_type = int64_t;
    static constexpr NamedColumn Column = NamedColumn::c_discount;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_balance> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int64_t;
    using value_type = int64_t;
    static constexpr NamedColumn Column = NamedColumn::c_balance;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_ytd_payment> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int64_t;
    using value_type = int64_t;
    static constexpr NamedColumn Column = NamedColumn::c_ytd_payment;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_payment_cnt> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint16_t;
    using value_type = uint16_t;
    static constexpr NamedColumn Column = NamedColumn::c_payment_cnt;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_delivery_cnt> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint16_t;
    using value_type = uint16_t;
    static constexpr NamedColumn Column = NamedColumn::c_delivery_cnt;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_data> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = fix_string<500>;
    using value_type = fix_string<500>;
    static constexpr NamedColumn Column = NamedColumn::c_data;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            customer_value* ptr) {
        if constexpr (NamedColumn::c_first <= Column && Column < NamedColumn::c_middle) {
            return ptr->c_first;
        }
        if constexpr (NamedColumn::c_middle <= Column && Column < NamedColumn::c_last) {
            return ptr->c_middle;
        }
        if constexpr (NamedColumn::c_last <= Column && Column < NamedColumn::c_street_1) {
            return ptr->c_last;
        }
        if constexpr (NamedColumn::c_street_1 <= Column && Column < NamedColumn::c_street_2) {
            return ptr->c_street_1;
        }
        if constexpr (NamedColumn::c_street_2 <= Column && Column < NamedColumn::c_city) {
            return ptr->c_street_2;
        }
        if constexpr (NamedColumn::c_city <= Column && Column < NamedColumn::c_state) {
            return ptr->c_city;
        }
        if constexpr (NamedColumn::c_state <= Column && Column < NamedColumn::c_zip) {
            return ptr->c_state;
        }
        if constexpr (NamedColumn::c_zip <= Column && Column < NamedColumn::c_phone) {
            return ptr->c_zip;
        }
        if constexpr (NamedColumn::c_phone <= Column && Column < NamedColumn::c_since) {
            return ptr->c_phone;
        }
        if constexpr (NamedColumn::c_since <= Column && Column < NamedColumn::c_credit) {
            return ptr->c_since;
        }
        if constexpr (NamedColumn::c_credit <= Column && Column < NamedColumn::c_credit_lim) {
            return ptr->c_credit;
        }
        if constexpr (NamedColumn::c_credit_lim <= Column && Column < NamedColumn::c_discount) {
            return ptr->c_credit_lim;
        }
        if constexpr (NamedColumn::c_discount <= Column && Column < NamedColumn::c_balance) {
            return ptr->c_discount;
        }
        if constexpr (NamedColumn::c_balance <= Column && Column < NamedColumn::c_ytd_payment) {
            return ptr->c_balance;
        }
        if constexpr (NamedColumn::c_ytd_payment <= Column && Column < NamedColumn::c_payment_cnt) {
            return ptr->c_ytd_payment;
        }
        if constexpr (NamedColumn::c_payment_cnt <= Column && Column < NamedColumn::c_delivery_cnt) {
            return ptr->c_payment_cnt;
        }
        if constexpr (NamedColumn::c_delivery_cnt <= Column && Column < NamedColumn::c_data) {
            return ptr->c_delivery_cnt;
        }
        if constexpr (NamedColumn::c_data <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->c_data;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 18;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(customer_value* dest, customer_value* src) {
        if constexpr(Cell == 0) {
            *dest = *src;
        }
        (void) dest; (void) src;
    }
};

template <>
struct SplitPolicy<1> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 1) {
            return 13;
        }
        if (cell == 0) {
            return 5;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(customer_value* dest, customer_value* src) {
        if constexpr(Cell == 1) {
            dest->c_first = src->c_first;
            dest->c_middle = src->c_middle;
            dest->c_last = src->c_last;
            dest->c_street_1 = src->c_street_1;
            dest->c_street_2 = src->c_street_2;
            dest->c_city = src->c_city;
            dest->c_state = src->c_state;
            dest->c_zip = src->c_zip;
            dest->c_phone = src->c_phone;
            dest->c_since = src->c_since;
            dest->c_credit = src->c_credit;
            dest->c_credit_lim = src->c_credit_lim;
            dest->c_discount = src->c_discount;
        }
        if constexpr(Cell == 0) {
            dest->c_balance = src->c_balance;
            dest->c_ytd_payment = src->c_ytd_payment;
            dest->c_payment_cnt = src->c_payment_cnt;
            dest->c_delivery_cnt = src->c_delivery_cnt;
            dest->c_data = src->c_data;
        }
        (void) dest; (void) src;
    }
};

class RecordAccessor {
public:
    using NamedColumn = customer_value_datatypes::NamedColumn;
    //using SplitTable = customer_value_datatypes::SplitTable;
    using SplitType = customer_value_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<customer_value>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = customer_value;
    static constexpr auto DEFAULT_SPLIT = 0;
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

    inline void copy_into(customer_value* vptr, int index) {
        if (vptrs_[0]) {
            copy_split_cell<0>(index, vptr, vptrs_[0]);
        }
        if (vptrs_[1]) {
            copy_split_cell<1>(index, vptr, vptrs_[1]);
        }
    }

    inline void copy_into(customer_value* vptr) {
        copy_into(vptr, splitindex_);
    }
    inline typename accessor_info<NamedColumn::c_first>::value_type& c_first() {
        return vptrs_[cell_of(NamedColumn::c_first)]->c_first;
    }

    inline const typename accessor_info<NamedColumn::c_first>::value_type& c_first() const {
        return vptrs_[cell_of(NamedColumn::c_first)]->c_first;
    }

    inline typename accessor_info<NamedColumn::c_middle>::value_type& c_middle() {
        return vptrs_[cell_of(NamedColumn::c_middle)]->c_middle;
    }

    inline const typename accessor_info<NamedColumn::c_middle>::value_type& c_middle() const {
        return vptrs_[cell_of(NamedColumn::c_middle)]->c_middle;
    }

    inline typename accessor_info<NamedColumn::c_last>::value_type& c_last() {
        return vptrs_[cell_of(NamedColumn::c_last)]->c_last;
    }

    inline const typename accessor_info<NamedColumn::c_last>::value_type& c_last() const {
        return vptrs_[cell_of(NamedColumn::c_last)]->c_last;
    }

    inline typename accessor_info<NamedColumn::c_street_1>::value_type& c_street_1() {
        return vptrs_[cell_of(NamedColumn::c_street_1)]->c_street_1;
    }

    inline const typename accessor_info<NamedColumn::c_street_1>::value_type& c_street_1() const {
        return vptrs_[cell_of(NamedColumn::c_street_1)]->c_street_1;
    }

    inline typename accessor_info<NamedColumn::c_street_2>::value_type& c_street_2() {
        return vptrs_[cell_of(NamedColumn::c_street_2)]->c_street_2;
    }

    inline const typename accessor_info<NamedColumn::c_street_2>::value_type& c_street_2() const {
        return vptrs_[cell_of(NamedColumn::c_street_2)]->c_street_2;
    }

    inline typename accessor_info<NamedColumn::c_city>::value_type& c_city() {
        return vptrs_[cell_of(NamedColumn::c_city)]->c_city;
    }

    inline const typename accessor_info<NamedColumn::c_city>::value_type& c_city() const {
        return vptrs_[cell_of(NamedColumn::c_city)]->c_city;
    }

    inline typename accessor_info<NamedColumn::c_state>::value_type& c_state() {
        return vptrs_[cell_of(NamedColumn::c_state)]->c_state;
    }

    inline const typename accessor_info<NamedColumn::c_state>::value_type& c_state() const {
        return vptrs_[cell_of(NamedColumn::c_state)]->c_state;
    }

    inline typename accessor_info<NamedColumn::c_zip>::value_type& c_zip() {
        return vptrs_[cell_of(NamedColumn::c_zip)]->c_zip;
    }

    inline const typename accessor_info<NamedColumn::c_zip>::value_type& c_zip() const {
        return vptrs_[cell_of(NamedColumn::c_zip)]->c_zip;
    }

    inline typename accessor_info<NamedColumn::c_phone>::value_type& c_phone() {
        return vptrs_[cell_of(NamedColumn::c_phone)]->c_phone;
    }

    inline const typename accessor_info<NamedColumn::c_phone>::value_type& c_phone() const {
        return vptrs_[cell_of(NamedColumn::c_phone)]->c_phone;
    }

    inline typename accessor_info<NamedColumn::c_since>::value_type& c_since() {
        return vptrs_[cell_of(NamedColumn::c_since)]->c_since;
    }

    inline const typename accessor_info<NamedColumn::c_since>::value_type& c_since() const {
        return vptrs_[cell_of(NamedColumn::c_since)]->c_since;
    }

    inline typename accessor_info<NamedColumn::c_credit>::value_type& c_credit() {
        return vptrs_[cell_of(NamedColumn::c_credit)]->c_credit;
    }

    inline const typename accessor_info<NamedColumn::c_credit>::value_type& c_credit() const {
        return vptrs_[cell_of(NamedColumn::c_credit)]->c_credit;
    }

    inline typename accessor_info<NamedColumn::c_credit_lim>::value_type& c_credit_lim() {
        return vptrs_[cell_of(NamedColumn::c_credit_lim)]->c_credit_lim;
    }

    inline const typename accessor_info<NamedColumn::c_credit_lim>::value_type& c_credit_lim() const {
        return vptrs_[cell_of(NamedColumn::c_credit_lim)]->c_credit_lim;
    }

    inline typename accessor_info<NamedColumn::c_discount>::value_type& c_discount() {
        return vptrs_[cell_of(NamedColumn::c_discount)]->c_discount;
    }

    inline const typename accessor_info<NamedColumn::c_discount>::value_type& c_discount() const {
        return vptrs_[cell_of(NamedColumn::c_discount)]->c_discount;
    }

    inline typename accessor_info<NamedColumn::c_balance>::value_type& c_balance() {
        return vptrs_[cell_of(NamedColumn::c_balance)]->c_balance;
    }

    inline const typename accessor_info<NamedColumn::c_balance>::value_type& c_balance() const {
        return vptrs_[cell_of(NamedColumn::c_balance)]->c_balance;
    }

    inline typename accessor_info<NamedColumn::c_ytd_payment>::value_type& c_ytd_payment() {
        return vptrs_[cell_of(NamedColumn::c_ytd_payment)]->c_ytd_payment;
    }

    inline const typename accessor_info<NamedColumn::c_ytd_payment>::value_type& c_ytd_payment() const {
        return vptrs_[cell_of(NamedColumn::c_ytd_payment)]->c_ytd_payment;
    }

    inline typename accessor_info<NamedColumn::c_payment_cnt>::value_type& c_payment_cnt() {
        return vptrs_[cell_of(NamedColumn::c_payment_cnt)]->c_payment_cnt;
    }

    inline const typename accessor_info<NamedColumn::c_payment_cnt>::value_type& c_payment_cnt() const {
        return vptrs_[cell_of(NamedColumn::c_payment_cnt)]->c_payment_cnt;
    }

    inline typename accessor_info<NamedColumn::c_delivery_cnt>::value_type& c_delivery_cnt() {
        return vptrs_[cell_of(NamedColumn::c_delivery_cnt)]->c_delivery_cnt;
    }

    inline const typename accessor_info<NamedColumn::c_delivery_cnt>::value_type& c_delivery_cnt() const {
        return vptrs_[cell_of(NamedColumn::c_delivery_cnt)]->c_delivery_cnt;
    }

    inline typename accessor_info<NamedColumn::c_data>::value_type& c_data() {
        return vptrs_[cell_of(NamedColumn::c_data)]->c_data;
    }

    inline const typename accessor_info<NamedColumn::c_data>::value_type& c_data() const {
        return vptrs_[cell_of(NamedColumn::c_data)]->c_data;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::c_first <= Column && Column < NamedColumn::c_middle) {
            return vptrs_[cell_of(NamedColumn::c_first)]->c_first;
        }
        if constexpr (NamedColumn::c_middle <= Column && Column < NamedColumn::c_last) {
            return vptrs_[cell_of(NamedColumn::c_middle)]->c_middle;
        }
        if constexpr (NamedColumn::c_last <= Column && Column < NamedColumn::c_street_1) {
            return vptrs_[cell_of(NamedColumn::c_last)]->c_last;
        }
        if constexpr (NamedColumn::c_street_1 <= Column && Column < NamedColumn::c_street_2) {
            return vptrs_[cell_of(NamedColumn::c_street_1)]->c_street_1;
        }
        if constexpr (NamedColumn::c_street_2 <= Column && Column < NamedColumn::c_city) {
            return vptrs_[cell_of(NamedColumn::c_street_2)]->c_street_2;
        }
        if constexpr (NamedColumn::c_city <= Column && Column < NamedColumn::c_state) {
            return vptrs_[cell_of(NamedColumn::c_city)]->c_city;
        }
        if constexpr (NamedColumn::c_state <= Column && Column < NamedColumn::c_zip) {
            return vptrs_[cell_of(NamedColumn::c_state)]->c_state;
        }
        if constexpr (NamedColumn::c_zip <= Column && Column < NamedColumn::c_phone) {
            return vptrs_[cell_of(NamedColumn::c_zip)]->c_zip;
        }
        if constexpr (NamedColumn::c_phone <= Column && Column < NamedColumn::c_since) {
            return vptrs_[cell_of(NamedColumn::c_phone)]->c_phone;
        }
        if constexpr (NamedColumn::c_since <= Column && Column < NamedColumn::c_credit) {
            return vptrs_[cell_of(NamedColumn::c_since)]->c_since;
        }
        if constexpr (NamedColumn::c_credit <= Column && Column < NamedColumn::c_credit_lim) {
            return vptrs_[cell_of(NamedColumn::c_credit)]->c_credit;
        }
        if constexpr (NamedColumn::c_credit_lim <= Column && Column < NamedColumn::c_discount) {
            return vptrs_[cell_of(NamedColumn::c_credit_lim)]->c_credit_lim;
        }
        if constexpr (NamedColumn::c_discount <= Column && Column < NamedColumn::c_balance) {
            return vptrs_[cell_of(NamedColumn::c_discount)]->c_discount;
        }
        if constexpr (NamedColumn::c_balance <= Column && Column < NamedColumn::c_ytd_payment) {
            return vptrs_[cell_of(NamedColumn::c_balance)]->c_balance;
        }
        if constexpr (NamedColumn::c_ytd_payment <= Column && Column < NamedColumn::c_payment_cnt) {
            return vptrs_[cell_of(NamedColumn::c_ytd_payment)]->c_ytd_payment;
        }
        if constexpr (NamedColumn::c_payment_cnt <= Column && Column < NamedColumn::c_delivery_cnt) {
            return vptrs_[cell_of(NamedColumn::c_payment_cnt)]->c_payment_cnt;
        }
        if constexpr (NamedColumn::c_delivery_cnt <= Column && Column < NamedColumn::c_data) {
            return vptrs_[cell_of(NamedColumn::c_delivery_cnt)]->c_delivery_cnt;
        }
        if constexpr (NamedColumn::c_data <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::c_data)]->c_data;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<customer_value*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace customer_value_datatypes

using customer_value = customer_value_datatypes::customer_value;

namespace history_value_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct history_value {
    using RecordAccessor = history_value_datatypes::RecordAccessor;
    using NamedColumn = history_value_datatypes::NamedColumn;

    uint64_t h_c_id;
    uint64_t h_c_d_id;
    uint64_t h_c_w_id;
    uint64_t h_d_id;
    uint64_t h_w_id;
    uint32_t h_date;
    int64_t h_amount;
    var_string<24> h_data;
};

enum class NamedColumn : int {
    h_c_id = 0,
    h_c_d_id,
    h_c_w_id,
    h_d_id,
    h_w_id,
    h_date,
    h_amount,
    h_data,
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
    if constexpr (Column < NamedColumn::h_c_d_id) {
        return NamedColumn::h_c_id;
    }
    if constexpr (Column < NamedColumn::h_c_w_id) {
        return NamedColumn::h_c_d_id;
    }
    if constexpr (Column < NamedColumn::h_d_id) {
        return NamedColumn::h_c_w_id;
    }
    if constexpr (Column < NamedColumn::h_w_id) {
        return NamedColumn::h_d_id;
    }
    if constexpr (Column < NamedColumn::h_date) {
        return NamedColumn::h_w_id;
    }
    if constexpr (Column < NamedColumn::h_amount) {
        return NamedColumn::h_date;
    }
    if constexpr (Column < NamedColumn::h_data) {
        return NamedColumn::h_amount;
    }
    return NamedColumn::h_data;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::h_c_id> {
    using NamedColumn = history_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::h_c_id;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::h_c_d_id> {
    using NamedColumn = history_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::h_c_d_id;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::h_c_w_id> {
    using NamedColumn = history_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::h_c_w_id;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::h_d_id> {
    using NamedColumn = history_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::h_d_id;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::h_w_id> {
    using NamedColumn = history_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::h_w_id;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::h_date> {
    using NamedColumn = history_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::h_date;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::h_amount> {
    using NamedColumn = history_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int64_t;
    using value_type = int64_t;
    static constexpr NamedColumn Column = NamedColumn::h_amount;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::h_data> {
    using NamedColumn = history_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<24>;
    using value_type = var_string<24>;
    static constexpr NamedColumn Column = NamedColumn::h_data;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            history_value* ptr) {
        if constexpr (NamedColumn::h_c_id <= Column && Column < NamedColumn::h_c_d_id) {
            return ptr->h_c_id;
        }
        if constexpr (NamedColumn::h_c_d_id <= Column && Column < NamedColumn::h_c_w_id) {
            return ptr->h_c_d_id;
        }
        if constexpr (NamedColumn::h_c_w_id <= Column && Column < NamedColumn::h_d_id) {
            return ptr->h_c_w_id;
        }
        if constexpr (NamedColumn::h_d_id <= Column && Column < NamedColumn::h_w_id) {
            return ptr->h_d_id;
        }
        if constexpr (NamedColumn::h_w_id <= Column && Column < NamedColumn::h_date) {
            return ptr->h_w_id;
        }
        if constexpr (NamedColumn::h_date <= Column && Column < NamedColumn::h_amount) {
            return ptr->h_date;
        }
        if constexpr (NamedColumn::h_amount <= Column && Column < NamedColumn::h_data) {
            return ptr->h_amount;
        }
        if constexpr (NamedColumn::h_data <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->h_data;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 8;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(history_value* dest, history_value* src) {
        if constexpr(Cell == 0) {
            *dest = *src;
        }
        (void) dest; (void) src;
    }
};

class RecordAccessor {
public:
    using NamedColumn = history_value_datatypes::NamedColumn;
    //using SplitTable = history_value_datatypes::SplitTable;
    using SplitType = history_value_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<history_value>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = history_value;
    static constexpr auto DEFAULT_SPLIT = 0;
    static constexpr auto MAX_SPLITS = 2;
    static constexpr auto MAX_POINTERS = MAX_SPLITS;
    static constexpr auto POLICIES = 1;

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
        return 0;
    }

    inline static constexpr const auto split_of(int index, NamedColumn column) {
        if (index == 0) {
            return SplitPolicy<0>::column_to_cell(column);
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
    }

    template <int Cell>
    inline static constexpr void copy_split_cell(int index, ValueType* dest, ValueType* src) {
        if (index == 0) {
            SplitPolicy<0>::copy_cell<Cell>(dest, src);
            return;
        }
    }

    inline static constexpr size_t cell_col_count(int index, int cell) {
        if (index == 0) {
            return SplitPolicy<0>::cell_col_count(cell);
        }
        return 0;
    }

    inline void copy_into(history_value* vptr, int index) {
        if (vptrs_[0]) {
            copy_split_cell<0>(index, vptr, vptrs_[0]);
        }
    }

    inline void copy_into(history_value* vptr) {
        copy_into(vptr, splitindex_);
    }
    inline typename accessor_info<NamedColumn::h_c_id>::value_type& h_c_id() {
        return vptrs_[cell_of(NamedColumn::h_c_id)]->h_c_id;
    }

    inline const typename accessor_info<NamedColumn::h_c_id>::value_type& h_c_id() const {
        return vptrs_[cell_of(NamedColumn::h_c_id)]->h_c_id;
    }

    inline typename accessor_info<NamedColumn::h_c_d_id>::value_type& h_c_d_id() {
        return vptrs_[cell_of(NamedColumn::h_c_d_id)]->h_c_d_id;
    }

    inline const typename accessor_info<NamedColumn::h_c_d_id>::value_type& h_c_d_id() const {
        return vptrs_[cell_of(NamedColumn::h_c_d_id)]->h_c_d_id;
    }

    inline typename accessor_info<NamedColumn::h_c_w_id>::value_type& h_c_w_id() {
        return vptrs_[cell_of(NamedColumn::h_c_w_id)]->h_c_w_id;
    }

    inline const typename accessor_info<NamedColumn::h_c_w_id>::value_type& h_c_w_id() const {
        return vptrs_[cell_of(NamedColumn::h_c_w_id)]->h_c_w_id;
    }

    inline typename accessor_info<NamedColumn::h_d_id>::value_type& h_d_id() {
        return vptrs_[cell_of(NamedColumn::h_d_id)]->h_d_id;
    }

    inline const typename accessor_info<NamedColumn::h_d_id>::value_type& h_d_id() const {
        return vptrs_[cell_of(NamedColumn::h_d_id)]->h_d_id;
    }

    inline typename accessor_info<NamedColumn::h_w_id>::value_type& h_w_id() {
        return vptrs_[cell_of(NamedColumn::h_w_id)]->h_w_id;
    }

    inline const typename accessor_info<NamedColumn::h_w_id>::value_type& h_w_id() const {
        return vptrs_[cell_of(NamedColumn::h_w_id)]->h_w_id;
    }

    inline typename accessor_info<NamedColumn::h_date>::value_type& h_date() {
        return vptrs_[cell_of(NamedColumn::h_date)]->h_date;
    }

    inline const typename accessor_info<NamedColumn::h_date>::value_type& h_date() const {
        return vptrs_[cell_of(NamedColumn::h_date)]->h_date;
    }

    inline typename accessor_info<NamedColumn::h_amount>::value_type& h_amount() {
        return vptrs_[cell_of(NamedColumn::h_amount)]->h_amount;
    }

    inline const typename accessor_info<NamedColumn::h_amount>::value_type& h_amount() const {
        return vptrs_[cell_of(NamedColumn::h_amount)]->h_amount;
    }

    inline typename accessor_info<NamedColumn::h_data>::value_type& h_data() {
        return vptrs_[cell_of(NamedColumn::h_data)]->h_data;
    }

    inline const typename accessor_info<NamedColumn::h_data>::value_type& h_data() const {
        return vptrs_[cell_of(NamedColumn::h_data)]->h_data;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::h_c_id <= Column && Column < NamedColumn::h_c_d_id) {
            return vptrs_[cell_of(NamedColumn::h_c_id)]->h_c_id;
        }
        if constexpr (NamedColumn::h_c_d_id <= Column && Column < NamedColumn::h_c_w_id) {
            return vptrs_[cell_of(NamedColumn::h_c_d_id)]->h_c_d_id;
        }
        if constexpr (NamedColumn::h_c_w_id <= Column && Column < NamedColumn::h_d_id) {
            return vptrs_[cell_of(NamedColumn::h_c_w_id)]->h_c_w_id;
        }
        if constexpr (NamedColumn::h_d_id <= Column && Column < NamedColumn::h_w_id) {
            return vptrs_[cell_of(NamedColumn::h_d_id)]->h_d_id;
        }
        if constexpr (NamedColumn::h_w_id <= Column && Column < NamedColumn::h_date) {
            return vptrs_[cell_of(NamedColumn::h_w_id)]->h_w_id;
        }
        if constexpr (NamedColumn::h_date <= Column && Column < NamedColumn::h_amount) {
            return vptrs_[cell_of(NamedColumn::h_date)]->h_date;
        }
        if constexpr (NamedColumn::h_amount <= Column && Column < NamedColumn::h_data) {
            return vptrs_[cell_of(NamedColumn::h_amount)]->h_amount;
        }
        if constexpr (NamedColumn::h_data <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::h_data)]->h_data;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<history_value*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace history_value_datatypes

using history_value = history_value_datatypes::history_value;

namespace order_value_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct order_value {
    using RecordAccessor = order_value_datatypes::RecordAccessor;
    using NamedColumn = order_value_datatypes::NamedColumn;

    uint64_t o_c_id;
    uint32_t o_entry_d;
    uint32_t o_ol_cnt;
    uint32_t o_all_local;
    uint64_t o_carrier_id;
};

enum class NamedColumn : int {
    o_c_id = 0,
    o_entry_d,
    o_ol_cnt,
    o_all_local,
    o_carrier_id,
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
    if constexpr (Column < NamedColumn::o_entry_d) {
        return NamedColumn::o_c_id;
    }
    if constexpr (Column < NamedColumn::o_ol_cnt) {
        return NamedColumn::o_entry_d;
    }
    if constexpr (Column < NamedColumn::o_all_local) {
        return NamedColumn::o_ol_cnt;
    }
    if constexpr (Column < NamedColumn::o_carrier_id) {
        return NamedColumn::o_all_local;
    }
    return NamedColumn::o_carrier_id;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::o_c_id> {
    using NamedColumn = order_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::o_c_id;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::o_entry_d> {
    using NamedColumn = order_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::o_entry_d;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::o_ol_cnt> {
    using NamedColumn = order_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::o_ol_cnt;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::o_all_local> {
    using NamedColumn = order_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::o_all_local;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::o_carrier_id> {
    using NamedColumn = order_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::o_carrier_id;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            order_value* ptr) {
        if constexpr (NamedColumn::o_c_id <= Column && Column < NamedColumn::o_entry_d) {
            return ptr->o_c_id;
        }
        if constexpr (NamedColumn::o_entry_d <= Column && Column < NamedColumn::o_ol_cnt) {
            return ptr->o_entry_d;
        }
        if constexpr (NamedColumn::o_ol_cnt <= Column && Column < NamedColumn::o_all_local) {
            return ptr->o_ol_cnt;
        }
        if constexpr (NamedColumn::o_all_local <= Column && Column < NamedColumn::o_carrier_id) {
            return ptr->o_all_local;
        }
        if constexpr (NamedColumn::o_carrier_id <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->o_carrier_id;
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
    inline static constexpr void copy_cell(order_value* dest, order_value* src) {
        if constexpr(Cell == 0) {
            *dest = *src;
        }
        (void) dest; (void) src;
    }
};

template <>
struct SplitPolicy<1> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0, 1 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 4;
        }
        if (cell == 1) {
            return 1;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(order_value* dest, order_value* src) {
        if constexpr(Cell == 0) {
            dest->o_c_id = src->o_c_id;
            dest->o_entry_d = src->o_entry_d;
            dest->o_ol_cnt = src->o_ol_cnt;
            dest->o_all_local = src->o_all_local;
        }
        if constexpr(Cell == 1) {
            dest->o_carrier_id = src->o_carrier_id;
        }
        (void) dest; (void) src;
    }
};

class RecordAccessor {
public:
    using NamedColumn = order_value_datatypes::NamedColumn;
    //using SplitTable = order_value_datatypes::SplitTable;
    using SplitType = order_value_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<order_value>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = order_value;
    static constexpr auto DEFAULT_SPLIT = 0;
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

    inline void copy_into(order_value* vptr, int index) {
        if (vptrs_[0]) {
            copy_split_cell<0>(index, vptr, vptrs_[0]);
        }
        if (vptrs_[1]) {
            copy_split_cell<1>(index, vptr, vptrs_[1]);
        }
    }

    inline void copy_into(order_value* vptr) {
        copy_into(vptr, splitindex_);
    }
    inline typename accessor_info<NamedColumn::o_c_id>::value_type& o_c_id() {
        return vptrs_[cell_of(NamedColumn::o_c_id)]->o_c_id;
    }

    inline const typename accessor_info<NamedColumn::o_c_id>::value_type& o_c_id() const {
        return vptrs_[cell_of(NamedColumn::o_c_id)]->o_c_id;
    }

    inline typename accessor_info<NamedColumn::o_entry_d>::value_type& o_entry_d() {
        return vptrs_[cell_of(NamedColumn::o_entry_d)]->o_entry_d;
    }

    inline const typename accessor_info<NamedColumn::o_entry_d>::value_type& o_entry_d() const {
        return vptrs_[cell_of(NamedColumn::o_entry_d)]->o_entry_d;
    }

    inline typename accessor_info<NamedColumn::o_ol_cnt>::value_type& o_ol_cnt() {
        return vptrs_[cell_of(NamedColumn::o_ol_cnt)]->o_ol_cnt;
    }

    inline const typename accessor_info<NamedColumn::o_ol_cnt>::value_type& o_ol_cnt() const {
        return vptrs_[cell_of(NamedColumn::o_ol_cnt)]->o_ol_cnt;
    }

    inline typename accessor_info<NamedColumn::o_all_local>::value_type& o_all_local() {
        return vptrs_[cell_of(NamedColumn::o_all_local)]->o_all_local;
    }

    inline const typename accessor_info<NamedColumn::o_all_local>::value_type& o_all_local() const {
        return vptrs_[cell_of(NamedColumn::o_all_local)]->o_all_local;
    }

    inline typename accessor_info<NamedColumn::o_carrier_id>::value_type& o_carrier_id() {
        return vptrs_[cell_of(NamedColumn::o_carrier_id)]->o_carrier_id;
    }

    inline const typename accessor_info<NamedColumn::o_carrier_id>::value_type& o_carrier_id() const {
        return vptrs_[cell_of(NamedColumn::o_carrier_id)]->o_carrier_id;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::o_c_id <= Column && Column < NamedColumn::o_entry_d) {
            return vptrs_[cell_of(NamedColumn::o_c_id)]->o_c_id;
        }
        if constexpr (NamedColumn::o_entry_d <= Column && Column < NamedColumn::o_ol_cnt) {
            return vptrs_[cell_of(NamedColumn::o_entry_d)]->o_entry_d;
        }
        if constexpr (NamedColumn::o_ol_cnt <= Column && Column < NamedColumn::o_all_local) {
            return vptrs_[cell_of(NamedColumn::o_ol_cnt)]->o_ol_cnt;
        }
        if constexpr (NamedColumn::o_all_local <= Column && Column < NamedColumn::o_carrier_id) {
            return vptrs_[cell_of(NamedColumn::o_all_local)]->o_all_local;
        }
        if constexpr (NamedColumn::o_carrier_id <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::o_carrier_id)]->o_carrier_id;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<order_value*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace order_value_datatypes

using order_value = order_value_datatypes::order_value;

namespace orderline_value_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct orderline_value {
    using RecordAccessor = orderline_value_datatypes::RecordAccessor;
    using NamedColumn = orderline_value_datatypes::NamedColumn;

    uint64_t ol_i_id;
    uint64_t ol_supply_w_id;
    uint32_t ol_quantity;
    int32_t ol_amount;
    fix_string<24> ol_dist_info;
    uint32_t ol_delivery_d;
};

enum class NamedColumn : int {
    ol_i_id = 0,
    ol_supply_w_id,
    ol_quantity,
    ol_amount,
    ol_dist_info,
    ol_delivery_d,
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
    if constexpr (Column < NamedColumn::ol_supply_w_id) {
        return NamedColumn::ol_i_id;
    }
    if constexpr (Column < NamedColumn::ol_quantity) {
        return NamedColumn::ol_supply_w_id;
    }
    if constexpr (Column < NamedColumn::ol_amount) {
        return NamedColumn::ol_quantity;
    }
    if constexpr (Column < NamedColumn::ol_dist_info) {
        return NamedColumn::ol_amount;
    }
    if constexpr (Column < NamedColumn::ol_delivery_d) {
        return NamedColumn::ol_dist_info;
    }
    return NamedColumn::ol_delivery_d;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::ol_i_id> {
    using NamedColumn = orderline_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::ol_i_id;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::ol_supply_w_id> {
    using NamedColumn = orderline_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::ol_supply_w_id;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::ol_quantity> {
    using NamedColumn = orderline_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::ol_quantity;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::ol_amount> {
    using NamedColumn = orderline_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::ol_amount;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::ol_dist_info> {
    using NamedColumn = orderline_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = fix_string<24>;
    using value_type = fix_string<24>;
    static constexpr NamedColumn Column = NamedColumn::ol_dist_info;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::ol_delivery_d> {
    using NamedColumn = orderline_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::ol_delivery_d;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            orderline_value* ptr) {
        if constexpr (NamedColumn::ol_i_id <= Column && Column < NamedColumn::ol_supply_w_id) {
            return ptr->ol_i_id;
        }
        if constexpr (NamedColumn::ol_supply_w_id <= Column && Column < NamedColumn::ol_quantity) {
            return ptr->ol_supply_w_id;
        }
        if constexpr (NamedColumn::ol_quantity <= Column && Column < NamedColumn::ol_amount) {
            return ptr->ol_quantity;
        }
        if constexpr (NamedColumn::ol_amount <= Column && Column < NamedColumn::ol_dist_info) {
            return ptr->ol_amount;
        }
        if constexpr (NamedColumn::ol_dist_info <= Column && Column < NamedColumn::ol_delivery_d) {
            return ptr->ol_dist_info;
        }
        if constexpr (NamedColumn::ol_delivery_d <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->ol_delivery_d;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 6;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(orderline_value* dest, orderline_value* src) {
        if constexpr(Cell == 0) {
            *dest = *src;
        }
        (void) dest; (void) src;
    }
};

template <>
struct SplitPolicy<1> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0, 0, 1 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 5;
        }
        if (cell == 1) {
            return 1;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(orderline_value* dest, orderline_value* src) {
        if constexpr(Cell == 0) {
            dest->ol_i_id = src->ol_i_id;
            dest->ol_supply_w_id = src->ol_supply_w_id;
            dest->ol_quantity = src->ol_quantity;
            dest->ol_amount = src->ol_amount;
            dest->ol_dist_info = src->ol_dist_info;
        }
        if constexpr(Cell == 1) {
            dest->ol_delivery_d = src->ol_delivery_d;
        }
        (void) dest; (void) src;
    }
};

class RecordAccessor {
public:
    using NamedColumn = orderline_value_datatypes::NamedColumn;
    //using SplitTable = orderline_value_datatypes::SplitTable;
    using SplitType = orderline_value_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<orderline_value>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = orderline_value;
    static constexpr auto DEFAULT_SPLIT = 0;
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

    inline void copy_into(orderline_value* vptr, int index) {
        if (vptrs_[0]) {
            copy_split_cell<0>(index, vptr, vptrs_[0]);
        }
        if (vptrs_[1]) {
            copy_split_cell<1>(index, vptr, vptrs_[1]);
        }
    }

    inline void copy_into(orderline_value* vptr) {
        copy_into(vptr, splitindex_);
    }
    inline typename accessor_info<NamedColumn::ol_i_id>::value_type& ol_i_id() {
        return vptrs_[cell_of(NamedColumn::ol_i_id)]->ol_i_id;
    }

    inline const typename accessor_info<NamedColumn::ol_i_id>::value_type& ol_i_id() const {
        return vptrs_[cell_of(NamedColumn::ol_i_id)]->ol_i_id;
    }

    inline typename accessor_info<NamedColumn::ol_supply_w_id>::value_type& ol_supply_w_id() {
        return vptrs_[cell_of(NamedColumn::ol_supply_w_id)]->ol_supply_w_id;
    }

    inline const typename accessor_info<NamedColumn::ol_supply_w_id>::value_type& ol_supply_w_id() const {
        return vptrs_[cell_of(NamedColumn::ol_supply_w_id)]->ol_supply_w_id;
    }

    inline typename accessor_info<NamedColumn::ol_quantity>::value_type& ol_quantity() {
        return vptrs_[cell_of(NamedColumn::ol_quantity)]->ol_quantity;
    }

    inline const typename accessor_info<NamedColumn::ol_quantity>::value_type& ol_quantity() const {
        return vptrs_[cell_of(NamedColumn::ol_quantity)]->ol_quantity;
    }

    inline typename accessor_info<NamedColumn::ol_amount>::value_type& ol_amount() {
        return vptrs_[cell_of(NamedColumn::ol_amount)]->ol_amount;
    }

    inline const typename accessor_info<NamedColumn::ol_amount>::value_type& ol_amount() const {
        return vptrs_[cell_of(NamedColumn::ol_amount)]->ol_amount;
    }

    inline typename accessor_info<NamedColumn::ol_dist_info>::value_type& ol_dist_info() {
        return vptrs_[cell_of(NamedColumn::ol_dist_info)]->ol_dist_info;
    }

    inline const typename accessor_info<NamedColumn::ol_dist_info>::value_type& ol_dist_info() const {
        return vptrs_[cell_of(NamedColumn::ol_dist_info)]->ol_dist_info;
    }

    inline typename accessor_info<NamedColumn::ol_delivery_d>::value_type& ol_delivery_d() {
        return vptrs_[cell_of(NamedColumn::ol_delivery_d)]->ol_delivery_d;
    }

    inline const typename accessor_info<NamedColumn::ol_delivery_d>::value_type& ol_delivery_d() const {
        return vptrs_[cell_of(NamedColumn::ol_delivery_d)]->ol_delivery_d;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::ol_i_id <= Column && Column < NamedColumn::ol_supply_w_id) {
            return vptrs_[cell_of(NamedColumn::ol_i_id)]->ol_i_id;
        }
        if constexpr (NamedColumn::ol_supply_w_id <= Column && Column < NamedColumn::ol_quantity) {
            return vptrs_[cell_of(NamedColumn::ol_supply_w_id)]->ol_supply_w_id;
        }
        if constexpr (NamedColumn::ol_quantity <= Column && Column < NamedColumn::ol_amount) {
            return vptrs_[cell_of(NamedColumn::ol_quantity)]->ol_quantity;
        }
        if constexpr (NamedColumn::ol_amount <= Column && Column < NamedColumn::ol_dist_info) {
            return vptrs_[cell_of(NamedColumn::ol_amount)]->ol_amount;
        }
        if constexpr (NamedColumn::ol_dist_info <= Column && Column < NamedColumn::ol_delivery_d) {
            return vptrs_[cell_of(NamedColumn::ol_dist_info)]->ol_dist_info;
        }
        if constexpr (NamedColumn::ol_delivery_d <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::ol_delivery_d)]->ol_delivery_d;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<orderline_value*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace orderline_value_datatypes

using orderline_value = orderline_value_datatypes::orderline_value;

namespace item_value_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct item_value {
    using RecordAccessor = item_value_datatypes::RecordAccessor;
    using NamedColumn = item_value_datatypes::NamedColumn;

    uint64_t i_im_id;
    uint32_t i_price;
    var_string<24> i_name;
    var_string<50> i_data;
};

enum class NamedColumn : int {
    i_im_id = 0,
    i_price,
    i_name,
    i_data,
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
    if constexpr (Column < NamedColumn::i_price) {
        return NamedColumn::i_im_id;
    }
    if constexpr (Column < NamedColumn::i_name) {
        return NamedColumn::i_price;
    }
    if constexpr (Column < NamedColumn::i_data) {
        return NamedColumn::i_name;
    }
    return NamedColumn::i_data;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::i_im_id> {
    using NamedColumn = item_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::i_im_id;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::i_price> {
    using NamedColumn = item_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::i_price;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::i_name> {
    using NamedColumn = item_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<24>;
    using value_type = var_string<24>;
    static constexpr NamedColumn Column = NamedColumn::i_name;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::i_data> {
    using NamedColumn = item_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<50>;
    using value_type = var_string<50>;
    static constexpr NamedColumn Column = NamedColumn::i_data;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            item_value* ptr) {
        if constexpr (NamedColumn::i_im_id <= Column && Column < NamedColumn::i_price) {
            return ptr->i_im_id;
        }
        if constexpr (NamedColumn::i_price <= Column && Column < NamedColumn::i_name) {
            return ptr->i_price;
        }
        if constexpr (NamedColumn::i_name <= Column && Column < NamedColumn::i_data) {
            return ptr->i_name;
        }
        if constexpr (NamedColumn::i_data <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->i_data;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 4;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(item_value* dest, item_value* src) {
        if constexpr(Cell == 0) {
            *dest = *src;
        }
        (void) dest; (void) src;
    }
};

class RecordAccessor {
public:
    using NamedColumn = item_value_datatypes::NamedColumn;
    //using SplitTable = item_value_datatypes::SplitTable;
    using SplitType = item_value_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<item_value>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = item_value;
    static constexpr auto DEFAULT_SPLIT = 0;
    static constexpr auto MAX_SPLITS = 2;
    static constexpr auto MAX_POINTERS = MAX_SPLITS;
    static constexpr auto POLICIES = 1;

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
        return 0;
    }

    inline static constexpr const auto split_of(int index, NamedColumn column) {
        if (index == 0) {
            return SplitPolicy<0>::column_to_cell(column);
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
    }

    template <int Cell>
    inline static constexpr void copy_split_cell(int index, ValueType* dest, ValueType* src) {
        if (index == 0) {
            SplitPolicy<0>::copy_cell<Cell>(dest, src);
            return;
        }
    }

    inline static constexpr size_t cell_col_count(int index, int cell) {
        if (index == 0) {
            return SplitPolicy<0>::cell_col_count(cell);
        }
        return 0;
    }

    inline void copy_into(item_value* vptr, int index) {
        if (vptrs_[0]) {
            copy_split_cell<0>(index, vptr, vptrs_[0]);
        }
    }

    inline void copy_into(item_value* vptr) {
        copy_into(vptr, splitindex_);
    }
    inline typename accessor_info<NamedColumn::i_im_id>::value_type& i_im_id() {
        return vptrs_[cell_of(NamedColumn::i_im_id)]->i_im_id;
    }

    inline const typename accessor_info<NamedColumn::i_im_id>::value_type& i_im_id() const {
        return vptrs_[cell_of(NamedColumn::i_im_id)]->i_im_id;
    }

    inline typename accessor_info<NamedColumn::i_price>::value_type& i_price() {
        return vptrs_[cell_of(NamedColumn::i_price)]->i_price;
    }

    inline const typename accessor_info<NamedColumn::i_price>::value_type& i_price() const {
        return vptrs_[cell_of(NamedColumn::i_price)]->i_price;
    }

    inline typename accessor_info<NamedColumn::i_name>::value_type& i_name() {
        return vptrs_[cell_of(NamedColumn::i_name)]->i_name;
    }

    inline const typename accessor_info<NamedColumn::i_name>::value_type& i_name() const {
        return vptrs_[cell_of(NamedColumn::i_name)]->i_name;
    }

    inline typename accessor_info<NamedColumn::i_data>::value_type& i_data() {
        return vptrs_[cell_of(NamedColumn::i_data)]->i_data;
    }

    inline const typename accessor_info<NamedColumn::i_data>::value_type& i_data() const {
        return vptrs_[cell_of(NamedColumn::i_data)]->i_data;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::i_im_id <= Column && Column < NamedColumn::i_price) {
            return vptrs_[cell_of(NamedColumn::i_im_id)]->i_im_id;
        }
        if constexpr (NamedColumn::i_price <= Column && Column < NamedColumn::i_name) {
            return vptrs_[cell_of(NamedColumn::i_price)]->i_price;
        }
        if constexpr (NamedColumn::i_name <= Column && Column < NamedColumn::i_data) {
            return vptrs_[cell_of(NamedColumn::i_name)]->i_name;
        }
        if constexpr (NamedColumn::i_data <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::i_data)]->i_data;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<item_value*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace item_value_datatypes

using item_value = item_value_datatypes::item_value;

namespace stock_value_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct stock_value {
    using RecordAccessor = stock_value_datatypes::RecordAccessor;
    using NamedColumn = stock_value_datatypes::NamedColumn;

    std::array<fix_string<24>, 10> s_dists;
    var_string<50> s_data;
    int32_t s_quantity;
    uint32_t s_ytd;
    uint32_t s_order_cnt;
    uint32_t s_remote_cnt;
};

enum class NamedColumn : int {
    s_dists = 0,
    s_data,
    s_quantity,
    s_ytd,
    s_order_cnt,
    s_remote_cnt,
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
    if constexpr (Column < NamedColumn::s_data) {
        return NamedColumn::s_dists;
    }
    if constexpr (Column < NamedColumn::s_quantity) {
        return NamedColumn::s_data;
    }
    if constexpr (Column < NamedColumn::s_ytd) {
        return NamedColumn::s_quantity;
    }
    if constexpr (Column < NamedColumn::s_order_cnt) {
        return NamedColumn::s_ytd;
    }
    if constexpr (Column < NamedColumn::s_remote_cnt) {
        return NamedColumn::s_order_cnt;
    }
    return NamedColumn::s_remote_cnt;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::s_dists> {
    using NamedColumn = stock_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = std::array<fix_string<24>, 10>;
    using value_type = std::array<fix_string<24>, 10>;
    static constexpr NamedColumn Column = NamedColumn::s_dists;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::s_data> {
    using NamedColumn = stock_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<50>;
    using value_type = var_string<50>;
    static constexpr NamedColumn Column = NamedColumn::s_data;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::s_quantity> {
    using NamedColumn = stock_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::s_quantity;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::s_ytd> {
    using NamedColumn = stock_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::s_ytd;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::s_order_cnt> {
    using NamedColumn = stock_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::s_order_cnt;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::s_remote_cnt> {
    using NamedColumn = stock_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::s_remote_cnt;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            stock_value* ptr) {
        if constexpr (NamedColumn::s_dists <= Column && Column < NamedColumn::s_data) {
            return ptr->s_dists;
        }
        if constexpr (NamedColumn::s_data <= Column && Column < NamedColumn::s_quantity) {
            return ptr->s_data;
        }
        if constexpr (NamedColumn::s_quantity <= Column && Column < NamedColumn::s_ytd) {
            return ptr->s_quantity;
        }
        if constexpr (NamedColumn::s_ytd <= Column && Column < NamedColumn::s_order_cnt) {
            return ptr->s_ytd;
        }
        if constexpr (NamedColumn::s_order_cnt <= Column && Column < NamedColumn::s_remote_cnt) {
            return ptr->s_order_cnt;
        }
        if constexpr (NamedColumn::s_remote_cnt <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->s_remote_cnt;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 6;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(stock_value* dest, stock_value* src) {
        if constexpr(Cell == 0) {
            *dest = *src;
        }
        (void) dest; (void) src;
    }
};

template <>
struct SplitPolicy<1> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 1, 1, 1, 1 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 2;
        }
        if (cell == 1) {
            return 4;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(stock_value* dest, stock_value* src) {
        if constexpr(Cell == 0) {
            dest->s_dists = src->s_dists;
            dest->s_data = src->s_data;
        }
        if constexpr(Cell == 1) {
            dest->s_quantity = src->s_quantity;
            dest->s_ytd = src->s_ytd;
            dest->s_order_cnt = src->s_order_cnt;
            dest->s_remote_cnt = src->s_remote_cnt;
        }
        (void) dest; (void) src;
    }
};

class RecordAccessor {
public:
    using NamedColumn = stock_value_datatypes::NamedColumn;
    //using SplitTable = stock_value_datatypes::SplitTable;
    using SplitType = stock_value_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<stock_value>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = stock_value;
    static constexpr auto DEFAULT_SPLIT = 0;
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

    inline void copy_into(stock_value* vptr, int index) {
        if (vptrs_[0]) {
            copy_split_cell<0>(index, vptr, vptrs_[0]);
        }
        if (vptrs_[1]) {
            copy_split_cell<1>(index, vptr, vptrs_[1]);
        }
    }

    inline void copy_into(stock_value* vptr) {
        copy_into(vptr, splitindex_);
    }
    inline typename accessor_info<NamedColumn::s_dists>::value_type& s_dists() {
        return vptrs_[cell_of(NamedColumn::s_dists)]->s_dists;
    }

    inline const typename accessor_info<NamedColumn::s_dists>::value_type& s_dists() const {
        return vptrs_[cell_of(NamedColumn::s_dists)]->s_dists;
    }

    inline typename accessor_info<NamedColumn::s_data>::value_type& s_data() {
        return vptrs_[cell_of(NamedColumn::s_data)]->s_data;
    }

    inline const typename accessor_info<NamedColumn::s_data>::value_type& s_data() const {
        return vptrs_[cell_of(NamedColumn::s_data)]->s_data;
    }

    inline typename accessor_info<NamedColumn::s_quantity>::value_type& s_quantity() {
        return vptrs_[cell_of(NamedColumn::s_quantity)]->s_quantity;
    }

    inline const typename accessor_info<NamedColumn::s_quantity>::value_type& s_quantity() const {
        return vptrs_[cell_of(NamedColumn::s_quantity)]->s_quantity;
    }

    inline typename accessor_info<NamedColumn::s_ytd>::value_type& s_ytd() {
        return vptrs_[cell_of(NamedColumn::s_ytd)]->s_ytd;
    }

    inline const typename accessor_info<NamedColumn::s_ytd>::value_type& s_ytd() const {
        return vptrs_[cell_of(NamedColumn::s_ytd)]->s_ytd;
    }

    inline typename accessor_info<NamedColumn::s_order_cnt>::value_type& s_order_cnt() {
        return vptrs_[cell_of(NamedColumn::s_order_cnt)]->s_order_cnt;
    }

    inline const typename accessor_info<NamedColumn::s_order_cnt>::value_type& s_order_cnt() const {
        return vptrs_[cell_of(NamedColumn::s_order_cnt)]->s_order_cnt;
    }

    inline typename accessor_info<NamedColumn::s_remote_cnt>::value_type& s_remote_cnt() {
        return vptrs_[cell_of(NamedColumn::s_remote_cnt)]->s_remote_cnt;
    }

    inline const typename accessor_info<NamedColumn::s_remote_cnt>::value_type& s_remote_cnt() const {
        return vptrs_[cell_of(NamedColumn::s_remote_cnt)]->s_remote_cnt;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::s_dists <= Column && Column < NamedColumn::s_data) {
            return vptrs_[cell_of(NamedColumn::s_dists)]->s_dists;
        }
        if constexpr (NamedColumn::s_data <= Column && Column < NamedColumn::s_quantity) {
            return vptrs_[cell_of(NamedColumn::s_data)]->s_data;
        }
        if constexpr (NamedColumn::s_quantity <= Column && Column < NamedColumn::s_ytd) {
            return vptrs_[cell_of(NamedColumn::s_quantity)]->s_quantity;
        }
        if constexpr (NamedColumn::s_ytd <= Column && Column < NamedColumn::s_order_cnt) {
            return vptrs_[cell_of(NamedColumn::s_ytd)]->s_ytd;
        }
        if constexpr (NamedColumn::s_order_cnt <= Column && Column < NamedColumn::s_remote_cnt) {
            return vptrs_[cell_of(NamedColumn::s_order_cnt)]->s_order_cnt;
        }
        if constexpr (NamedColumn::s_remote_cnt <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::s_remote_cnt)]->s_remote_cnt;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<stock_value*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace stock_value_datatypes

using stock_value = stock_value_datatypes::stock_value;

