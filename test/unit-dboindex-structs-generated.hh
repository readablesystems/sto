#pragma once

#include <type_traits>
//#include "AdapterStructs.hh"

namespace coarse_grained_row_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct coarse_grained_row {
    using RecordAccessor = coarse_grained_row_datatypes::RecordAccessor;
    using NamedColumn = coarse_grained_row_datatypes::NamedColumn;

    uint64_t aa;
    uint64_t bb;
    uint64_t cc;
};

enum class NamedColumn : int {
    aa = 0,
    bb,
    cc,
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
    if constexpr (Column < NamedColumn::bb) {
        return NamedColumn::aa;
    }
    if constexpr (Column < NamedColumn::cc) {
        return NamedColumn::bb;
    }
    return NamedColumn::cc;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::aa> {
    using NamedColumn = coarse_grained_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::aa;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::bb> {
    using NamedColumn = coarse_grained_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::bb;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::cc> {
    using NamedColumn = coarse_grained_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::cc;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            coarse_grained_row* ptr) {
        if constexpr (NamedColumn::aa <= Column && Column < NamedColumn::bb) {
            return ptr->aa;
        }
        if constexpr (NamedColumn::bb <= Column && Column < NamedColumn::cc) {
            return ptr->bb;
        }
        if constexpr (NamedColumn::cc <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->cc;
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
    inline static constexpr void copy_cell(coarse_grained_row* dest, coarse_grained_row* src) {
        if constexpr(Cell == 0) {
            dest->aa = src->aa;
            dest->bb = src->bb;
            dest->cc = src->cc;
        }
    }
};

class RecordAccessor {
public:
    using NamedColumn = coarse_grained_row_datatypes::NamedColumn;
    //using SplitTable = coarse_grained_row_datatypes::SplitTable;
    using SplitType = coarse_grained_row_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<coarse_grained_row>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = coarse_grained_row;
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

    inline static constexpr size_t cell_col_count(int index, int cell) {
        if (index == 0) {
            return SplitPolicy<0>::cell_col_count(cell);
        }
        return 0;
    }

    void copy_into(coarse_grained_row* vptr, int index=0) {
        copy_cell(index, 0, vptr, vptrs_[0]);
    }

    inline typename accessor_info<NamedColumn::aa>::value_type& aa() {
        return vptrs_[cell_of(NamedColumn::aa)]->aa;
    }

    inline const typename accessor_info<NamedColumn::aa>::value_type& aa() const {
        return vptrs_[cell_of(NamedColumn::aa)]->aa;
    }

    inline typename accessor_info<NamedColumn::bb>::value_type& bb() {
        return vptrs_[cell_of(NamedColumn::bb)]->bb;
    }

    inline const typename accessor_info<NamedColumn::bb>::value_type& bb() const {
        return vptrs_[cell_of(NamedColumn::bb)]->bb;
    }

    inline typename accessor_info<NamedColumn::cc>::value_type& cc() {
        return vptrs_[cell_of(NamedColumn::cc)]->cc;
    }

    inline const typename accessor_info<NamedColumn::cc>::value_type& cc() const {
        return vptrs_[cell_of(NamedColumn::cc)]->cc;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::aa <= Column && Column < NamedColumn::bb) {
            return vptrs_[cell_of(NamedColumn::aa)]->aa;
        }
        if constexpr (NamedColumn::bb <= Column && Column < NamedColumn::cc) {
            return vptrs_[cell_of(NamedColumn::bb)]->bb;
        }
        if constexpr (NamedColumn::cc <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::cc)]->cc;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<coarse_grained_row*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace coarse_grained_row_datatypes

using coarse_grained_row = coarse_grained_row_datatypes::coarse_grained_row;

