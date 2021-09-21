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

    std::array<double, 2> data;
    std::string label;
    bool flagged;
};

enum class NamedColumn : int {
    data = 0,
    label = 2,
    flagged,
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
    if constexpr (Column < NamedColumn::label) {
        return NamedColumn::data;
    }
    if constexpr (Column < NamedColumn::flagged) {
        return NamedColumn::label;
    }
    return NamedColumn::flagged;
}

struct SplitTable {
    static constexpr std::array<int, static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT)> Splits[3] = {
        { 0, 0, 0, 0 },
        { 1, 1, 0, 0 },
        { 0, 1, 0, 1 },
    };
};

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::data> {
    using NamedColumn = index_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = double;
    using value_type = std::array<double, 2>;
    static constexpr NamedColumn Column = NamedColumn::data;
    static constexpr bool is_array = true;
};

template <>
struct accessor_info<NamedColumn::label> {
    using NamedColumn = index_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = std::string;
    using value_type = std::string;
    static constexpr NamedColumn Column = NamedColumn::label;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::flagged> {
    using NamedColumn = index_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = bool;
    using value_type = bool;
    static constexpr NamedColumn Column = NamedColumn::flagged;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

class RecordAccessor {
public:
    using NamedColumn = index_value_datatypes::NamedColumn;
    using SplitType = index_value_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<index_value>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = index_value;
    static constexpr auto DEFAULT_SPLIT = 1;
    static constexpr auto MAX_SPLITS = 2;
    static constexpr auto MAX_POINTERS = MAX_SPLITS;

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

    const static auto split_of(int index, NamedColumn column) {
        return SplitTable::Splits[index][static_cast<std::underlying_type_t<NamedColumn>>(column)];
    }

    const auto cell_of(NamedColumn column) const {
        return split_of(splitindex_, column);
    }

    inline typename accessor_info<NamedColumn::data>::value_type& data() {
        return vptrs_[cell_of(NamedColumn::data)]->data;
    }

    inline typename accessor_info<NamedColumn::data>::type& data(
            const std::underlying_type_t<NamedColumn> index) {
        return vptrs_[cell_of(NamedColumn::data)]->data[index];
    }

    inline const typename accessor_info<NamedColumn::data>::value_type& data() const {
        return vptrs_[cell_of(NamedColumn::data)]->data;
    }

    inline const typename accessor_info<NamedColumn::data>::type& data(
            const std::underlying_type_t<NamedColumn> index) const {
        return vptrs_[cell_of(NamedColumn::data)]->data[index];
    }

    inline typename accessor_info<NamedColumn::label>::value_type& label() {
        return vptrs_[cell_of(NamedColumn::label)]->label;
    }

    inline const typename accessor_info<NamedColumn::label>::value_type& label() const {
        return vptrs_[cell_of(NamedColumn::label)]->label;
    }

    inline typename accessor_info<NamedColumn::flagged>::value_type& flagged() {
        return vptrs_[cell_of(NamedColumn::flagged)]->flagged;
    }

    inline const typename accessor_info<NamedColumn::flagged>::value_type& flagged() const {
        return vptrs_[cell_of(NamedColumn::flagged)]->flagged;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::data <= Column && Column < NamedColumn::label) {
            return vptrs_[cell_of(NamedColumn::data)]->data;
        }
        if constexpr (NamedColumn::label <= Column && Column < NamedColumn::flagged) {
            return vptrs_[cell_of(NamedColumn::label)]->label;
        }
        if constexpr (NamedColumn::flagged <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::flagged)]->flagged;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }

    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            index_value* ptr) {
        if constexpr (NamedColumn::data <= Column && Column < NamedColumn::label) {
            return ptr->data[static_cast<std::underlying_type_t<NamedColumn>>(Column - NamedColumn::data)];
        }
        if constexpr (NamedColumn::label <= Column && Column < NamedColumn::flagged) {
            return ptr->label;
        }
        if constexpr (NamedColumn::flagged <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->flagged;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }

    std::array<index_value*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace index_value_datatypes

using index_value = index_value_datatypes::index_value;

