#pragma once

#include <type_traits>

namespace index_value_datatypes {

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

struct index_value;

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::data> {
    using NamedColumn = index_value_datatypes::NamedColumn;
    using struct_type = index_value;
    using type = double;
    using value_type = std::array<double, 2>;
    static constexpr NamedColumn Column = NamedColumn::data;
    static constexpr bool is_array = true;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::label> {
    using NamedColumn = index_value_datatypes::NamedColumn;
    using struct_type = index_value;
    using type = std::string;
    using value_type = std::string;
    static constexpr NamedColumn Column = NamedColumn::label;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::flagged> {
    using NamedColumn = index_value_datatypes::NamedColumn;
    using struct_type = index_value;
    using type = bool;
    using value_type = bool;
    static constexpr NamedColumn Column = NamedColumn::flagged;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct index_value {
    using NamedColumn = index_value_datatypes::NamedColumn;
    static constexpr auto DEFAULT_SPLIT = NamedColumn::COLCOUNT;
    static constexpr auto MAX_SPLITS = 2;

    explicit index_value() = default;

    static inline void resplit(
            index_value& newvalue, const index_value& oldvalue, NamedColumn index);

    template <NamedColumn Column>
    static inline void copy_data(index_value& newvalue, const index_value& oldvalue);

    inline void init(const index_value* oldvalue = nullptr) {
        if (oldvalue) {
            auto split = oldvalue->splitindex_;
            if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Global)) {
                split = ADAPTER_OF(index_value)::CurrentSplit();
            }
            if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Inline)) {
                split = split;
            }
            index_value::resplit(*this, *oldvalue, split);
        }
    }

    template <NamedColumn Column>
    inline ::sto::adapter::Accessor<accessor_info<RoundedNamedColumn<Column>()>>& get();

    template <NamedColumn Column>
    inline const ::sto::adapter::Accessor<accessor_info<RoundedNamedColumn<Column>()>>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    ::sto::adapter::Accessor<accessor_info<NamedColumn::data>> data;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::label>> label;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::flagged>> flagged;
    NamedColumn splitindex_ = DEFAULT_SPLIT;
};

inline void index_value::resplit(
        index_value& newvalue, const index_value& oldvalue, NamedColumn index) {
    assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
    if (&newvalue != &oldvalue) {
        memcpy(&newvalue, &oldvalue, sizeof newvalue);
        //copy_data<NamedColumn(0)>(newvalue, oldvalue);
    }
    newvalue.splitindex_ = index;
}

template <NamedColumn Column>
inline void index_value::copy_data(index_value& newvalue, const index_value& oldvalue) {
    static_assert(Column < NamedColumn::COLCOUNT);
    newvalue.template get<Column>().value_ = oldvalue.template get<Column>().value_;
    if constexpr (Column + 1 < NamedColumn::COLCOUNT) {
        copy_data<Column + 1>(newvalue, oldvalue);
    }
}

inline size_t accessor_info<NamedColumn::data>::offset() {
    index_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->data) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::label>::offset() {
    index_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->label) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::flagged>::offset() {
    index_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->flagged) - reinterpret_cast<uintptr_t>(ptr));
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::data>>& index_value::get<NamedColumn::data + 0>() {
    return data;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::data>>& index_value::get<NamedColumn::data + 0>() const {
    return data;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::data>>& index_value::get<NamedColumn::data + 1>() {
    return data;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::data>>& index_value::get<NamedColumn::data + 1>() const {
    return data;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::label>>& index_value::get<NamedColumn::label>() {
    return label;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::label>>& index_value::get<NamedColumn::label>() const {
    return label;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::flagged>>& index_value::get<NamedColumn::flagged>() {
    return flagged;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::flagged>>& index_value::get<NamedColumn::flagged>() const {
    return flagged;
}

};  // namespace index_value_datatypes

using index_value = index_value_datatypes::index_value;

