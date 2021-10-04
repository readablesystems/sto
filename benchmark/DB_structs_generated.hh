#pragma once

#include <type_traits>
//#include "AdapterStructs.hh"

namespace dummy_row_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct dummy_row {
    using RecordAccessor = dummy_row_datatypes::RecordAccessor;
    using NamedColumn = dummy_row_datatypes::NamedColumn;

    static dummy_row row;

    uintptr_t dummy;
};

enum class NamedColumn : int {
    dummy = 0,
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
    return NamedColumn::dummy;
}

struct SplitTable {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr auto Size = 1;
    using SplitPolicy = int;
    static constexpr SplitPolicy Splits[Size][ColCount] = {
        { 0 },
    };
};

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::dummy> {
    using NamedColumn = dummy_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uintptr_t;
    using value_type = uintptr_t;
    static constexpr NamedColumn Column = NamedColumn::dummy;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

class RecordAccessor {
public:
    using NamedColumn = dummy_row_datatypes::NamedColumn;
    using SplitTable = dummy_row_datatypes::SplitTable;
    using SplitType = dummy_row_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<dummy_row>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = dummy_row;
    static constexpr auto DEFAULT_SPLIT = 0;
    static constexpr auto MAX_SPLITS = 2;
    static constexpr auto MAX_POINTERS = MAX_SPLITS;
    static constexpr auto POLICY_COUNT = SplitTable::Size;

    RecordAccessor() = default;
    template <typename... T>
    RecordAccessor(T ...vals) : vptrs_({ pointer_of(vals)... }) {}

    inline operator bool() const {
        return vptrs_ [0] != nullptr;
    }

    /*
    inline ValueType* pointer_of(ValueType& value) {
        return &value;
    }
    */

    inline ValueType* pointer_of(ValueType* vptr) {
        return vptr;
    }

    static constexpr const auto split_of(int index, NamedColumn column) {
        return SplitTable::Splits[index][static_cast<std::underlying_type_t<NamedColumn>>(column)];
    }

    const auto cell_of(NamedColumn column) const {
        return split_of(splitindex_, column);
    }

    void copy_into(dummy_row* vptr) {
        memcpy(vptr, vptrs_[0], sizeof *vptr);
    }

    inline typename accessor_info<NamedColumn::dummy>::value_type& dummy() {
        return vptrs_[cell_of(NamedColumn::dummy)]->dummy;
    }

    inline const typename accessor_info<NamedColumn::dummy>::value_type& dummy() const {
        return vptrs_[cell_of(NamedColumn::dummy)]->dummy;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::dummy <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::dummy)]->dummy;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }

    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            dummy_row* ptr) {
        if constexpr (NamedColumn::dummy <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->dummy;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }

    std::array<dummy_row*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace dummy_row_datatypes

using dummy_row = dummy_row_datatypes::dummy_row;

