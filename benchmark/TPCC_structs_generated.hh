#pragma once

#include <type_traits>

namespace warehouse_value_datatypes {

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

struct warehouse_value;

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::w_name> {
    using NamedColumn = warehouse_value_datatypes::NamedColumn;
    using struct_type = warehouse_value;
    using type = var_string<10>;
    using value_type = var_string<10>;
    static constexpr NamedColumn Column = NamedColumn::w_name;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::w_street_1> {
    using NamedColumn = warehouse_value_datatypes::NamedColumn;
    using struct_type = warehouse_value;
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr NamedColumn Column = NamedColumn::w_street_1;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::w_street_2> {
    using NamedColumn = warehouse_value_datatypes::NamedColumn;
    using struct_type = warehouse_value;
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr NamedColumn Column = NamedColumn::w_street_2;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::w_city> {
    using NamedColumn = warehouse_value_datatypes::NamedColumn;
    using struct_type = warehouse_value;
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr NamedColumn Column = NamedColumn::w_city;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::w_state> {
    using NamedColumn = warehouse_value_datatypes::NamedColumn;
    using struct_type = warehouse_value;
    using type = fix_string<2>;
    using value_type = fix_string<2>;
    static constexpr NamedColumn Column = NamedColumn::w_state;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::w_zip> {
    using NamedColumn = warehouse_value_datatypes::NamedColumn;
    using struct_type = warehouse_value;
    using type = fix_string<9>;
    using value_type = fix_string<9>;
    static constexpr NamedColumn Column = NamedColumn::w_zip;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::w_tax> {
    using NamedColumn = warehouse_value_datatypes::NamedColumn;
    using struct_type = warehouse_value;
    using type = int64_t;
    using value_type = int64_t;
    static constexpr NamedColumn Column = NamedColumn::w_tax;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::w_ytd> {
    using NamedColumn = warehouse_value_datatypes::NamedColumn;
    using struct_type = warehouse_value;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::w_ytd;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct warehouse_value {
    using NamedColumn = warehouse_value_datatypes::NamedColumn;
    static constexpr auto DEFAULT_SPLIT = NamedColumn::COLCOUNT;
    static constexpr auto MAX_SPLITS = 2;

    explicit warehouse_value() = default;

    static inline void resplit(
            warehouse_value& newvalue, const warehouse_value& oldvalue, NamedColumn index);

    template <NamedColumn Column>
    static inline void copy_data(warehouse_value& newvalue, const warehouse_value& oldvalue);

    inline void init(const warehouse_value* oldvalue = nullptr) {
        if (oldvalue) {
            auto split = oldvalue->splitindex_;
            if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Global)) {
                split = ADAPTER_OF(warehouse_value)::CurrentSplit();
            }
            if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Inline)) {
                split = split;
            }
            warehouse_value::resplit(*this, *oldvalue, split);
        }
    }

    template <NamedColumn Column>
    inline ::sto::adapter::Accessor<accessor_info<RoundedNamedColumn<Column>()>>& get();

    template <NamedColumn Column>
    inline const ::sto::adapter::Accessor<accessor_info<RoundedNamedColumn<Column>()>>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    ::sto::adapter::Accessor<accessor_info<NamedColumn::w_name>> w_name;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::w_street_1>> w_street_1;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::w_street_2>> w_street_2;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::w_city>> w_city;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::w_state>> w_state;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::w_zip>> w_zip;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::w_tax>> w_tax;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::w_ytd>> w_ytd;
    NamedColumn splitindex_ = DEFAULT_SPLIT;
};

inline void warehouse_value::resplit(
        warehouse_value& newvalue, const warehouse_value& oldvalue, NamedColumn index) {
    assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
    if (&newvalue != &oldvalue) {
        memcpy(&newvalue, &oldvalue, sizeof newvalue);
        //copy_data<NamedColumn(0)>(newvalue, oldvalue);
    }
    newvalue.splitindex_ = index;
}

template <NamedColumn Column>
inline void warehouse_value::copy_data(warehouse_value& newvalue, const warehouse_value& oldvalue) {
    static_assert(Column < NamedColumn::COLCOUNT);
    newvalue.template get<Column>().value_ = oldvalue.template get<Column>().value_;
    if constexpr (Column + 1 < NamedColumn::COLCOUNT) {
        copy_data<Column + 1>(newvalue, oldvalue);
    }
}

inline size_t accessor_info<NamedColumn::w_name>::offset() {
    warehouse_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->w_name) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::w_street_1>::offset() {
    warehouse_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->w_street_1) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::w_street_2>::offset() {
    warehouse_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->w_street_2) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::w_city>::offset() {
    warehouse_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->w_city) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::w_state>::offset() {
    warehouse_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->w_state) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::w_zip>::offset() {
    warehouse_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->w_zip) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::w_tax>::offset() {
    warehouse_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->w_tax) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::w_ytd>::offset() {
    warehouse_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->w_ytd) - reinterpret_cast<uintptr_t>(ptr));
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::w_name>>& warehouse_value::get<NamedColumn::w_name>() {
    return w_name;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::w_name>>& warehouse_value::get<NamedColumn::w_name>() const {
    return w_name;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::w_street_1>>& warehouse_value::get<NamedColumn::w_street_1>() {
    return w_street_1;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::w_street_1>>& warehouse_value::get<NamedColumn::w_street_1>() const {
    return w_street_1;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::w_street_2>>& warehouse_value::get<NamedColumn::w_street_2>() {
    return w_street_2;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::w_street_2>>& warehouse_value::get<NamedColumn::w_street_2>() const {
    return w_street_2;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::w_city>>& warehouse_value::get<NamedColumn::w_city>() {
    return w_city;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::w_city>>& warehouse_value::get<NamedColumn::w_city>() const {
    return w_city;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::w_state>>& warehouse_value::get<NamedColumn::w_state>() {
    return w_state;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::w_state>>& warehouse_value::get<NamedColumn::w_state>() const {
    return w_state;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::w_zip>>& warehouse_value::get<NamedColumn::w_zip>() {
    return w_zip;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::w_zip>>& warehouse_value::get<NamedColumn::w_zip>() const {
    return w_zip;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::w_tax>>& warehouse_value::get<NamedColumn::w_tax>() {
    return w_tax;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::w_tax>>& warehouse_value::get<NamedColumn::w_tax>() const {
    return w_tax;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::w_ytd>>& warehouse_value::get<NamedColumn::w_ytd>() {
    return w_ytd;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::w_ytd>>& warehouse_value::get<NamedColumn::w_ytd>() const {
    return w_ytd;
}

};  // namespace warehouse_value_datatypes

using warehouse_value = warehouse_value_datatypes::warehouse_value;

namespace district_value_datatypes {

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

struct district_value;

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::d_name> {
    using NamedColumn = district_value_datatypes::NamedColumn;
    using struct_type = district_value;
    using type = var_string<10>;
    using value_type = var_string<10>;
    static constexpr NamedColumn Column = NamedColumn::d_name;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::d_street_1> {
    using NamedColumn = district_value_datatypes::NamedColumn;
    using struct_type = district_value;
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr NamedColumn Column = NamedColumn::d_street_1;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::d_street_2> {
    using NamedColumn = district_value_datatypes::NamedColumn;
    using struct_type = district_value;
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr NamedColumn Column = NamedColumn::d_street_2;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::d_city> {
    using NamedColumn = district_value_datatypes::NamedColumn;
    using struct_type = district_value;
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr NamedColumn Column = NamedColumn::d_city;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::d_state> {
    using NamedColumn = district_value_datatypes::NamedColumn;
    using struct_type = district_value;
    using type = fix_string<2>;
    using value_type = fix_string<2>;
    static constexpr NamedColumn Column = NamedColumn::d_state;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::d_zip> {
    using NamedColumn = district_value_datatypes::NamedColumn;
    using struct_type = district_value;
    using type = fix_string<9>;
    using value_type = fix_string<9>;
    static constexpr NamedColumn Column = NamedColumn::d_zip;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::d_tax> {
    using NamedColumn = district_value_datatypes::NamedColumn;
    using struct_type = district_value;
    using type = int64_t;
    using value_type = int64_t;
    static constexpr NamedColumn Column = NamedColumn::d_tax;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::d_ytd> {
    using NamedColumn = district_value_datatypes::NamedColumn;
    using struct_type = district_value;
    using type = int64_t;
    using value_type = int64_t;
    static constexpr NamedColumn Column = NamedColumn::d_ytd;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct district_value {
    using NamedColumn = district_value_datatypes::NamedColumn;
    static constexpr auto DEFAULT_SPLIT = NamedColumn::COLCOUNT;
    static constexpr auto MAX_SPLITS = 2;

    explicit district_value() = default;

    static inline void resplit(
            district_value& newvalue, const district_value& oldvalue, NamedColumn index);

    template <NamedColumn Column>
    static inline void copy_data(district_value& newvalue, const district_value& oldvalue);

    inline void init(const district_value* oldvalue = nullptr) {
        if (oldvalue) {
            auto split = oldvalue->splitindex_;
            if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Global)) {
                split = ADAPTER_OF(district_value)::CurrentSplit();
            }
            if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Inline)) {
                split = split;
            }
            district_value::resplit(*this, *oldvalue, split);
        }
    }

    template <NamedColumn Column>
    inline ::sto::adapter::Accessor<accessor_info<RoundedNamedColumn<Column>()>>& get();

    template <NamedColumn Column>
    inline const ::sto::adapter::Accessor<accessor_info<RoundedNamedColumn<Column>()>>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    ::sto::adapter::Accessor<accessor_info<NamedColumn::d_name>> d_name;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::d_street_1>> d_street_1;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::d_street_2>> d_street_2;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::d_city>> d_city;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::d_state>> d_state;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::d_zip>> d_zip;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::d_tax>> d_tax;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::d_ytd>> d_ytd;
    NamedColumn splitindex_ = DEFAULT_SPLIT;
};

inline void district_value::resplit(
        district_value& newvalue, const district_value& oldvalue, NamedColumn index) {
    assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
    if (&newvalue != &oldvalue) {
        memcpy(&newvalue, &oldvalue, sizeof newvalue);
        //copy_data<NamedColumn(0)>(newvalue, oldvalue);
    }
    newvalue.splitindex_ = index;
}

template <NamedColumn Column>
inline void district_value::copy_data(district_value& newvalue, const district_value& oldvalue) {
    static_assert(Column < NamedColumn::COLCOUNT);
    newvalue.template get<Column>().value_ = oldvalue.template get<Column>().value_;
    if constexpr (Column + 1 < NamedColumn::COLCOUNT) {
        copy_data<Column + 1>(newvalue, oldvalue);
    }
}

inline size_t accessor_info<NamedColumn::d_name>::offset() {
    district_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->d_name) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::d_street_1>::offset() {
    district_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->d_street_1) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::d_street_2>::offset() {
    district_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->d_street_2) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::d_city>::offset() {
    district_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->d_city) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::d_state>::offset() {
    district_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->d_state) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::d_zip>::offset() {
    district_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->d_zip) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::d_tax>::offset() {
    district_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->d_tax) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::d_ytd>::offset() {
    district_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->d_ytd) - reinterpret_cast<uintptr_t>(ptr));
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::d_name>>& district_value::get<NamedColumn::d_name>() {
    return d_name;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::d_name>>& district_value::get<NamedColumn::d_name>() const {
    return d_name;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::d_street_1>>& district_value::get<NamedColumn::d_street_1>() {
    return d_street_1;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::d_street_1>>& district_value::get<NamedColumn::d_street_1>() const {
    return d_street_1;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::d_street_2>>& district_value::get<NamedColumn::d_street_2>() {
    return d_street_2;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::d_street_2>>& district_value::get<NamedColumn::d_street_2>() const {
    return d_street_2;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::d_city>>& district_value::get<NamedColumn::d_city>() {
    return d_city;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::d_city>>& district_value::get<NamedColumn::d_city>() const {
    return d_city;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::d_state>>& district_value::get<NamedColumn::d_state>() {
    return d_state;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::d_state>>& district_value::get<NamedColumn::d_state>() const {
    return d_state;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::d_zip>>& district_value::get<NamedColumn::d_zip>() {
    return d_zip;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::d_zip>>& district_value::get<NamedColumn::d_zip>() const {
    return d_zip;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::d_tax>>& district_value::get<NamedColumn::d_tax>() {
    return d_tax;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::d_tax>>& district_value::get<NamedColumn::d_tax>() const {
    return d_tax;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::d_ytd>>& district_value::get<NamedColumn::d_ytd>() {
    return d_ytd;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::d_ytd>>& district_value::get<NamedColumn::d_ytd>() const {
    return d_ytd;
}

};  // namespace district_value_datatypes

using district_value = district_value_datatypes::district_value;

namespace customer_idx_value_datatypes {

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

struct customer_idx_value;

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::c_ids> {
    using NamedColumn = customer_idx_value_datatypes::NamedColumn;
    using struct_type = customer_idx_value;
    using type = std::list<uint64_t>;
    using value_type = std::list<uint64_t>;
    static constexpr NamedColumn Column = NamedColumn::c_ids;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct customer_idx_value {
    using NamedColumn = customer_idx_value_datatypes::NamedColumn;
    static constexpr auto DEFAULT_SPLIT = NamedColumn::COLCOUNT;
    static constexpr auto MAX_SPLITS = 2;

    explicit customer_idx_value() = default;

    static inline void resplit(
            customer_idx_value& newvalue, const customer_idx_value& oldvalue, NamedColumn index);

    template <NamedColumn Column>
    static inline void copy_data(customer_idx_value& newvalue, const customer_idx_value& oldvalue);

    inline void init(const customer_idx_value* oldvalue = nullptr) {
        if (oldvalue) {
            auto split = oldvalue->splitindex_;
            if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Global)) {
                split = ADAPTER_OF(customer_idx_value)::CurrentSplit();
            }
            if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Inline)) {
                split = split;
            }
            customer_idx_value::resplit(*this, *oldvalue, split);
        }
    }

    template <NamedColumn Column>
    inline ::sto::adapter::Accessor<accessor_info<RoundedNamedColumn<Column>()>>& get();

    template <NamedColumn Column>
    inline const ::sto::adapter::Accessor<accessor_info<RoundedNamedColumn<Column>()>>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    ::sto::adapter::Accessor<accessor_info<NamedColumn::c_ids>> c_ids;
    NamedColumn splitindex_ = DEFAULT_SPLIT;
};

inline void customer_idx_value::resplit(
        customer_idx_value& newvalue, const customer_idx_value& oldvalue, NamedColumn index) {
    assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
    if (&newvalue != &oldvalue) {
        memcpy(&newvalue, &oldvalue, sizeof newvalue);
        //copy_data<NamedColumn(0)>(newvalue, oldvalue);
    }
    newvalue.splitindex_ = index;
}

template <NamedColumn Column>
inline void customer_idx_value::copy_data(customer_idx_value& newvalue, const customer_idx_value& oldvalue) {
    static_assert(Column < NamedColumn::COLCOUNT);
    newvalue.template get<Column>().value_ = oldvalue.template get<Column>().value_;
    if constexpr (Column + 1 < NamedColumn::COLCOUNT) {
        copy_data<Column + 1>(newvalue, oldvalue);
    }
}

inline size_t accessor_info<NamedColumn::c_ids>::offset() {
    customer_idx_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->c_ids) - reinterpret_cast<uintptr_t>(ptr));
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::c_ids>>& customer_idx_value::get<NamedColumn::c_ids>() {
    return c_ids;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::c_ids>>& customer_idx_value::get<NamedColumn::c_ids>() const {
    return c_ids;
}

};  // namespace customer_idx_value_datatypes

using customer_idx_value = customer_idx_value_datatypes::customer_idx_value;

namespace customer_value_datatypes {

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

struct customer_value;

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::c_first> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = customer_value;
    using type = var_string<16>;
    using value_type = var_string<16>;
    static constexpr NamedColumn Column = NamedColumn::c_first;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::c_middle> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = customer_value;
    using type = fix_string<2>;
    using value_type = fix_string<2>;
    static constexpr NamedColumn Column = NamedColumn::c_middle;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::c_last> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = customer_value;
    using type = var_string<16>;
    using value_type = var_string<16>;
    static constexpr NamedColumn Column = NamedColumn::c_last;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::c_street_1> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = customer_value;
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr NamedColumn Column = NamedColumn::c_street_1;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::c_street_2> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = customer_value;
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr NamedColumn Column = NamedColumn::c_street_2;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::c_city> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = customer_value;
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr NamedColumn Column = NamedColumn::c_city;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::c_state> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = customer_value;
    using type = fix_string<2>;
    using value_type = fix_string<2>;
    static constexpr NamedColumn Column = NamedColumn::c_state;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::c_zip> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = customer_value;
    using type = fix_string<9>;
    using value_type = fix_string<9>;
    static constexpr NamedColumn Column = NamedColumn::c_zip;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::c_phone> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = customer_value;
    using type = fix_string<16>;
    using value_type = fix_string<16>;
    static constexpr NamedColumn Column = NamedColumn::c_phone;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::c_since> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = customer_value;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::c_since;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::c_credit> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = customer_value;
    using type = fix_string<2>;
    using value_type = fix_string<2>;
    static constexpr NamedColumn Column = NamedColumn::c_credit;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::c_credit_lim> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = customer_value;
    using type = int64_t;
    using value_type = int64_t;
    static constexpr NamedColumn Column = NamedColumn::c_credit_lim;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::c_discount> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = customer_value;
    using type = int64_t;
    using value_type = int64_t;
    static constexpr NamedColumn Column = NamedColumn::c_discount;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::c_balance> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = customer_value;
    using type = int64_t;
    using value_type = int64_t;
    static constexpr NamedColumn Column = NamedColumn::c_balance;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::c_ytd_payment> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = customer_value;
    using type = int64_t;
    using value_type = int64_t;
    static constexpr NamedColumn Column = NamedColumn::c_ytd_payment;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::c_payment_cnt> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = customer_value;
    using type = uint16_t;
    using value_type = uint16_t;
    static constexpr NamedColumn Column = NamedColumn::c_payment_cnt;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::c_delivery_cnt> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = customer_value;
    using type = uint16_t;
    using value_type = uint16_t;
    static constexpr NamedColumn Column = NamedColumn::c_delivery_cnt;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::c_data> {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    using struct_type = customer_value;
    using type = fix_string<500>;
    using value_type = fix_string<500>;
    static constexpr NamedColumn Column = NamedColumn::c_data;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct customer_value {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    static constexpr auto DEFAULT_SPLIT = NamedColumn::COLCOUNT;
    static constexpr auto MAX_SPLITS = 2;

    explicit customer_value() = default;

    static inline void resplit(
            customer_value& newvalue, const customer_value& oldvalue, NamedColumn index);

    template <NamedColumn Column>
    static inline void copy_data(customer_value& newvalue, const customer_value& oldvalue);

    inline void init(const customer_value* oldvalue = nullptr) {
        if (oldvalue) {
            auto split = oldvalue->splitindex_;
            if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Global)) {
                split = ADAPTER_OF(customer_value)::CurrentSplit();
            }
            if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Inline)) {
                split = split;
            }
            customer_value::resplit(*this, *oldvalue, split);
        }
    }

    template <NamedColumn Column>
    inline ::sto::adapter::Accessor<accessor_info<RoundedNamedColumn<Column>()>>& get();

    template <NamedColumn Column>
    inline const ::sto::adapter::Accessor<accessor_info<RoundedNamedColumn<Column>()>>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    ::sto::adapter::Accessor<accessor_info<NamedColumn::c_first>> c_first;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::c_middle>> c_middle;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::c_last>> c_last;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::c_street_1>> c_street_1;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::c_street_2>> c_street_2;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::c_city>> c_city;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::c_state>> c_state;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::c_zip>> c_zip;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::c_phone>> c_phone;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::c_since>> c_since;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::c_credit>> c_credit;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::c_credit_lim>> c_credit_lim;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::c_discount>> c_discount;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::c_balance>> c_balance;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::c_ytd_payment>> c_ytd_payment;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::c_payment_cnt>> c_payment_cnt;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::c_delivery_cnt>> c_delivery_cnt;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::c_data>> c_data;
    NamedColumn splitindex_ = DEFAULT_SPLIT;
};

inline void customer_value::resplit(
        customer_value& newvalue, const customer_value& oldvalue, NamedColumn index) {
    assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
    if (&newvalue != &oldvalue) {
        memcpy(&newvalue, &oldvalue, sizeof newvalue);
        //copy_data<NamedColumn(0)>(newvalue, oldvalue);
    }
    newvalue.splitindex_ = index;
}

template <NamedColumn Column>
inline void customer_value::copy_data(customer_value& newvalue, const customer_value& oldvalue) {
    static_assert(Column < NamedColumn::COLCOUNT);
    newvalue.template get<Column>().value_ = oldvalue.template get<Column>().value_;
    if constexpr (Column + 1 < NamedColumn::COLCOUNT) {
        copy_data<Column + 1>(newvalue, oldvalue);
    }
}

inline size_t accessor_info<NamedColumn::c_first>::offset() {
    customer_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->c_first) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::c_middle>::offset() {
    customer_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->c_middle) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::c_last>::offset() {
    customer_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->c_last) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::c_street_1>::offset() {
    customer_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->c_street_1) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::c_street_2>::offset() {
    customer_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->c_street_2) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::c_city>::offset() {
    customer_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->c_city) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::c_state>::offset() {
    customer_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->c_state) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::c_zip>::offset() {
    customer_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->c_zip) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::c_phone>::offset() {
    customer_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->c_phone) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::c_since>::offset() {
    customer_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->c_since) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::c_credit>::offset() {
    customer_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->c_credit) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::c_credit_lim>::offset() {
    customer_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->c_credit_lim) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::c_discount>::offset() {
    customer_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->c_discount) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::c_balance>::offset() {
    customer_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->c_balance) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::c_ytd_payment>::offset() {
    customer_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->c_ytd_payment) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::c_payment_cnt>::offset() {
    customer_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->c_payment_cnt) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::c_delivery_cnt>::offset() {
    customer_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->c_delivery_cnt) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::c_data>::offset() {
    customer_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->c_data) - reinterpret_cast<uintptr_t>(ptr));
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::c_first>>& customer_value::get<NamedColumn::c_first>() {
    return c_first;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::c_first>>& customer_value::get<NamedColumn::c_first>() const {
    return c_first;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::c_middle>>& customer_value::get<NamedColumn::c_middle>() {
    return c_middle;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::c_middle>>& customer_value::get<NamedColumn::c_middle>() const {
    return c_middle;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::c_last>>& customer_value::get<NamedColumn::c_last>() {
    return c_last;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::c_last>>& customer_value::get<NamedColumn::c_last>() const {
    return c_last;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::c_street_1>>& customer_value::get<NamedColumn::c_street_1>() {
    return c_street_1;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::c_street_1>>& customer_value::get<NamedColumn::c_street_1>() const {
    return c_street_1;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::c_street_2>>& customer_value::get<NamedColumn::c_street_2>() {
    return c_street_2;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::c_street_2>>& customer_value::get<NamedColumn::c_street_2>() const {
    return c_street_2;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::c_city>>& customer_value::get<NamedColumn::c_city>() {
    return c_city;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::c_city>>& customer_value::get<NamedColumn::c_city>() const {
    return c_city;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::c_state>>& customer_value::get<NamedColumn::c_state>() {
    return c_state;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::c_state>>& customer_value::get<NamedColumn::c_state>() const {
    return c_state;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::c_zip>>& customer_value::get<NamedColumn::c_zip>() {
    return c_zip;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::c_zip>>& customer_value::get<NamedColumn::c_zip>() const {
    return c_zip;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::c_phone>>& customer_value::get<NamedColumn::c_phone>() {
    return c_phone;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::c_phone>>& customer_value::get<NamedColumn::c_phone>() const {
    return c_phone;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::c_since>>& customer_value::get<NamedColumn::c_since>() {
    return c_since;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::c_since>>& customer_value::get<NamedColumn::c_since>() const {
    return c_since;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::c_credit>>& customer_value::get<NamedColumn::c_credit>() {
    return c_credit;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::c_credit>>& customer_value::get<NamedColumn::c_credit>() const {
    return c_credit;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::c_credit_lim>>& customer_value::get<NamedColumn::c_credit_lim>() {
    return c_credit_lim;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::c_credit_lim>>& customer_value::get<NamedColumn::c_credit_lim>() const {
    return c_credit_lim;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::c_discount>>& customer_value::get<NamedColumn::c_discount>() {
    return c_discount;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::c_discount>>& customer_value::get<NamedColumn::c_discount>() const {
    return c_discount;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::c_balance>>& customer_value::get<NamedColumn::c_balance>() {
    return c_balance;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::c_balance>>& customer_value::get<NamedColumn::c_balance>() const {
    return c_balance;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::c_ytd_payment>>& customer_value::get<NamedColumn::c_ytd_payment>() {
    return c_ytd_payment;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::c_ytd_payment>>& customer_value::get<NamedColumn::c_ytd_payment>() const {
    return c_ytd_payment;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::c_payment_cnt>>& customer_value::get<NamedColumn::c_payment_cnt>() {
    return c_payment_cnt;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::c_payment_cnt>>& customer_value::get<NamedColumn::c_payment_cnt>() const {
    return c_payment_cnt;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::c_delivery_cnt>>& customer_value::get<NamedColumn::c_delivery_cnt>() {
    return c_delivery_cnt;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::c_delivery_cnt>>& customer_value::get<NamedColumn::c_delivery_cnt>() const {
    return c_delivery_cnt;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::c_data>>& customer_value::get<NamedColumn::c_data>() {
    return c_data;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::c_data>>& customer_value::get<NamedColumn::c_data>() const {
    return c_data;
}

};  // namespace customer_value_datatypes

using customer_value = customer_value_datatypes::customer_value;

namespace history_value_datatypes {

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

struct history_value;

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::h_c_id> {
    using NamedColumn = history_value_datatypes::NamedColumn;
    using struct_type = history_value;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::h_c_id;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::h_c_d_id> {
    using NamedColumn = history_value_datatypes::NamedColumn;
    using struct_type = history_value;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::h_c_d_id;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::h_c_w_id> {
    using NamedColumn = history_value_datatypes::NamedColumn;
    using struct_type = history_value;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::h_c_w_id;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::h_d_id> {
    using NamedColumn = history_value_datatypes::NamedColumn;
    using struct_type = history_value;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::h_d_id;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::h_w_id> {
    using NamedColumn = history_value_datatypes::NamedColumn;
    using struct_type = history_value;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::h_w_id;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::h_date> {
    using NamedColumn = history_value_datatypes::NamedColumn;
    using struct_type = history_value;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::h_date;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::h_amount> {
    using NamedColumn = history_value_datatypes::NamedColumn;
    using struct_type = history_value;
    using type = int64_t;
    using value_type = int64_t;
    static constexpr NamedColumn Column = NamedColumn::h_amount;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::h_data> {
    using NamedColumn = history_value_datatypes::NamedColumn;
    using struct_type = history_value;
    using type = var_string<24>;
    using value_type = var_string<24>;
    static constexpr NamedColumn Column = NamedColumn::h_data;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct history_value {
    using NamedColumn = history_value_datatypes::NamedColumn;
    static constexpr auto DEFAULT_SPLIT = NamedColumn::COLCOUNT;
    static constexpr auto MAX_SPLITS = 2;

    explicit history_value() = default;

    static inline void resplit(
            history_value& newvalue, const history_value& oldvalue, NamedColumn index);

    template <NamedColumn Column>
    static inline void copy_data(history_value& newvalue, const history_value& oldvalue);

    inline void init(const history_value* oldvalue = nullptr) {
        if (oldvalue) {
            auto split = oldvalue->splitindex_;
            if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Global)) {
                split = ADAPTER_OF(history_value)::CurrentSplit();
            }
            if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Inline)) {
                split = split;
            }
            history_value::resplit(*this, *oldvalue, split);
        }
    }

    template <NamedColumn Column>
    inline ::sto::adapter::Accessor<accessor_info<RoundedNamedColumn<Column>()>>& get();

    template <NamedColumn Column>
    inline const ::sto::adapter::Accessor<accessor_info<RoundedNamedColumn<Column>()>>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    ::sto::adapter::Accessor<accessor_info<NamedColumn::h_c_id>> h_c_id;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::h_c_d_id>> h_c_d_id;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::h_c_w_id>> h_c_w_id;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::h_d_id>> h_d_id;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::h_w_id>> h_w_id;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::h_date>> h_date;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::h_amount>> h_amount;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::h_data>> h_data;
    NamedColumn splitindex_ = DEFAULT_SPLIT;
};

inline void history_value::resplit(
        history_value& newvalue, const history_value& oldvalue, NamedColumn index) {
    assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
    if (&newvalue != &oldvalue) {
        memcpy(&newvalue, &oldvalue, sizeof newvalue);
        //copy_data<NamedColumn(0)>(newvalue, oldvalue);
    }
    newvalue.splitindex_ = index;
}

template <NamedColumn Column>
inline void history_value::copy_data(history_value& newvalue, const history_value& oldvalue) {
    static_assert(Column < NamedColumn::COLCOUNT);
    newvalue.template get<Column>().value_ = oldvalue.template get<Column>().value_;
    if constexpr (Column + 1 < NamedColumn::COLCOUNT) {
        copy_data<Column + 1>(newvalue, oldvalue);
    }
}

inline size_t accessor_info<NamedColumn::h_c_id>::offset() {
    history_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->h_c_id) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::h_c_d_id>::offset() {
    history_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->h_c_d_id) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::h_c_w_id>::offset() {
    history_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->h_c_w_id) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::h_d_id>::offset() {
    history_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->h_d_id) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::h_w_id>::offset() {
    history_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->h_w_id) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::h_date>::offset() {
    history_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->h_date) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::h_amount>::offset() {
    history_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->h_amount) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::h_data>::offset() {
    history_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->h_data) - reinterpret_cast<uintptr_t>(ptr));
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::h_c_id>>& history_value::get<NamedColumn::h_c_id>() {
    return h_c_id;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::h_c_id>>& history_value::get<NamedColumn::h_c_id>() const {
    return h_c_id;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::h_c_d_id>>& history_value::get<NamedColumn::h_c_d_id>() {
    return h_c_d_id;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::h_c_d_id>>& history_value::get<NamedColumn::h_c_d_id>() const {
    return h_c_d_id;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::h_c_w_id>>& history_value::get<NamedColumn::h_c_w_id>() {
    return h_c_w_id;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::h_c_w_id>>& history_value::get<NamedColumn::h_c_w_id>() const {
    return h_c_w_id;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::h_d_id>>& history_value::get<NamedColumn::h_d_id>() {
    return h_d_id;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::h_d_id>>& history_value::get<NamedColumn::h_d_id>() const {
    return h_d_id;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::h_w_id>>& history_value::get<NamedColumn::h_w_id>() {
    return h_w_id;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::h_w_id>>& history_value::get<NamedColumn::h_w_id>() const {
    return h_w_id;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::h_date>>& history_value::get<NamedColumn::h_date>() {
    return h_date;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::h_date>>& history_value::get<NamedColumn::h_date>() const {
    return h_date;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::h_amount>>& history_value::get<NamedColumn::h_amount>() {
    return h_amount;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::h_amount>>& history_value::get<NamedColumn::h_amount>() const {
    return h_amount;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::h_data>>& history_value::get<NamedColumn::h_data>() {
    return h_data;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::h_data>>& history_value::get<NamedColumn::h_data>() const {
    return h_data;
}

};  // namespace history_value_datatypes

using history_value = history_value_datatypes::history_value;

namespace order_value_datatypes {

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

struct order_value;

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::o_c_id> {
    using NamedColumn = order_value_datatypes::NamedColumn;
    using struct_type = order_value;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::o_c_id;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::o_entry_d> {
    using NamedColumn = order_value_datatypes::NamedColumn;
    using struct_type = order_value;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::o_entry_d;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::o_ol_cnt> {
    using NamedColumn = order_value_datatypes::NamedColumn;
    using struct_type = order_value;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::o_ol_cnt;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::o_all_local> {
    using NamedColumn = order_value_datatypes::NamedColumn;
    using struct_type = order_value;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::o_all_local;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::o_carrier_id> {
    using NamedColumn = order_value_datatypes::NamedColumn;
    using struct_type = order_value;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::o_carrier_id;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct order_value {
    using NamedColumn = order_value_datatypes::NamedColumn;
    static constexpr auto DEFAULT_SPLIT = NamedColumn::COLCOUNT;
    static constexpr auto MAX_SPLITS = 2;

    explicit order_value() = default;

    static inline void resplit(
            order_value& newvalue, const order_value& oldvalue, NamedColumn index);

    template <NamedColumn Column>
    static inline void copy_data(order_value& newvalue, const order_value& oldvalue);

    inline void init(const order_value* oldvalue = nullptr) {
        if (oldvalue) {
            auto split = oldvalue->splitindex_;
            if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Global)) {
                split = ADAPTER_OF(order_value)::CurrentSplit();
            }
            if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Inline)) {
                split = split;
            }
            order_value::resplit(*this, *oldvalue, split);
        }
    }

    template <NamedColumn Column>
    inline ::sto::adapter::Accessor<accessor_info<RoundedNamedColumn<Column>()>>& get();

    template <NamedColumn Column>
    inline const ::sto::adapter::Accessor<accessor_info<RoundedNamedColumn<Column>()>>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    ::sto::adapter::Accessor<accessor_info<NamedColumn::o_c_id>> o_c_id;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::o_entry_d>> o_entry_d;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::o_ol_cnt>> o_ol_cnt;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::o_all_local>> o_all_local;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::o_carrier_id>> o_carrier_id;
    NamedColumn splitindex_ = DEFAULT_SPLIT;
};

inline void order_value::resplit(
        order_value& newvalue, const order_value& oldvalue, NamedColumn index) {
    assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
    if (&newvalue != &oldvalue) {
        memcpy(&newvalue, &oldvalue, sizeof newvalue);
        //copy_data<NamedColumn(0)>(newvalue, oldvalue);
    }
    newvalue.splitindex_ = index;
}

template <NamedColumn Column>
inline void order_value::copy_data(order_value& newvalue, const order_value& oldvalue) {
    static_assert(Column < NamedColumn::COLCOUNT);
    newvalue.template get<Column>().value_ = oldvalue.template get<Column>().value_;
    if constexpr (Column + 1 < NamedColumn::COLCOUNT) {
        copy_data<Column + 1>(newvalue, oldvalue);
    }
}

inline size_t accessor_info<NamedColumn::o_c_id>::offset() {
    order_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->o_c_id) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::o_entry_d>::offset() {
    order_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->o_entry_d) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::o_ol_cnt>::offset() {
    order_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->o_ol_cnt) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::o_all_local>::offset() {
    order_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->o_all_local) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::o_carrier_id>::offset() {
    order_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->o_carrier_id) - reinterpret_cast<uintptr_t>(ptr));
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::o_c_id>>& order_value::get<NamedColumn::o_c_id>() {
    return o_c_id;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::o_c_id>>& order_value::get<NamedColumn::o_c_id>() const {
    return o_c_id;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::o_entry_d>>& order_value::get<NamedColumn::o_entry_d>() {
    return o_entry_d;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::o_entry_d>>& order_value::get<NamedColumn::o_entry_d>() const {
    return o_entry_d;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::o_ol_cnt>>& order_value::get<NamedColumn::o_ol_cnt>() {
    return o_ol_cnt;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::o_ol_cnt>>& order_value::get<NamedColumn::o_ol_cnt>() const {
    return o_ol_cnt;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::o_all_local>>& order_value::get<NamedColumn::o_all_local>() {
    return o_all_local;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::o_all_local>>& order_value::get<NamedColumn::o_all_local>() const {
    return o_all_local;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::o_carrier_id>>& order_value::get<NamedColumn::o_carrier_id>() {
    return o_carrier_id;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::o_carrier_id>>& order_value::get<NamedColumn::o_carrier_id>() const {
    return o_carrier_id;
}

};  // namespace order_value_datatypes

using order_value = order_value_datatypes::order_value;

namespace orderline_value_datatypes {

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

struct orderline_value;

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::ol_i_id> {
    using NamedColumn = orderline_value_datatypes::NamedColumn;
    using struct_type = orderline_value;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::ol_i_id;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::ol_supply_w_id> {
    using NamedColumn = orderline_value_datatypes::NamedColumn;
    using struct_type = orderline_value;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::ol_supply_w_id;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::ol_quantity> {
    using NamedColumn = orderline_value_datatypes::NamedColumn;
    using struct_type = orderline_value;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::ol_quantity;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::ol_amount> {
    using NamedColumn = orderline_value_datatypes::NamedColumn;
    using struct_type = orderline_value;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::ol_amount;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::ol_dist_info> {
    using NamedColumn = orderline_value_datatypes::NamedColumn;
    using struct_type = orderline_value;
    using type = fix_string<24>;
    using value_type = fix_string<24>;
    static constexpr NamedColumn Column = NamedColumn::ol_dist_info;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::ol_delivery_d> {
    using NamedColumn = orderline_value_datatypes::NamedColumn;
    using struct_type = orderline_value;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::ol_delivery_d;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct orderline_value {
    using NamedColumn = orderline_value_datatypes::NamedColumn;
    static constexpr auto DEFAULT_SPLIT = NamedColumn::COLCOUNT;
    static constexpr auto MAX_SPLITS = 2;

    explicit orderline_value() = default;

    static inline void resplit(
            orderline_value& newvalue, const orderline_value& oldvalue, NamedColumn index);

    template <NamedColumn Column>
    static inline void copy_data(orderline_value& newvalue, const orderline_value& oldvalue);

    inline void init(const orderline_value* oldvalue = nullptr) {
        if (oldvalue) {
            auto split = oldvalue->splitindex_;
            if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Global)) {
                split = ADAPTER_OF(orderline_value)::CurrentSplit();
            }
            if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Inline)) {
                split = split;
            }
            orderline_value::resplit(*this, *oldvalue, split);
        }
    }

    template <NamedColumn Column>
    inline ::sto::adapter::Accessor<accessor_info<RoundedNamedColumn<Column>()>>& get();

    template <NamedColumn Column>
    inline const ::sto::adapter::Accessor<accessor_info<RoundedNamedColumn<Column>()>>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    ::sto::adapter::Accessor<accessor_info<NamedColumn::ol_i_id>> ol_i_id;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::ol_supply_w_id>> ol_supply_w_id;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::ol_quantity>> ol_quantity;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::ol_amount>> ol_amount;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::ol_dist_info>> ol_dist_info;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::ol_delivery_d>> ol_delivery_d;
    NamedColumn splitindex_ = DEFAULT_SPLIT;
};

inline void orderline_value::resplit(
        orderline_value& newvalue, const orderline_value& oldvalue, NamedColumn index) {
    assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
    if (&newvalue != &oldvalue) {
        memcpy(&newvalue, &oldvalue, sizeof newvalue);
        //copy_data<NamedColumn(0)>(newvalue, oldvalue);
    }
    newvalue.splitindex_ = index;
}

template <NamedColumn Column>
inline void orderline_value::copy_data(orderline_value& newvalue, const orderline_value& oldvalue) {
    static_assert(Column < NamedColumn::COLCOUNT);
    newvalue.template get<Column>().value_ = oldvalue.template get<Column>().value_;
    if constexpr (Column + 1 < NamedColumn::COLCOUNT) {
        copy_data<Column + 1>(newvalue, oldvalue);
    }
}

inline size_t accessor_info<NamedColumn::ol_i_id>::offset() {
    orderline_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->ol_i_id) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::ol_supply_w_id>::offset() {
    orderline_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->ol_supply_w_id) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::ol_quantity>::offset() {
    orderline_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->ol_quantity) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::ol_amount>::offset() {
    orderline_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->ol_amount) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::ol_dist_info>::offset() {
    orderline_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->ol_dist_info) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::ol_delivery_d>::offset() {
    orderline_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->ol_delivery_d) - reinterpret_cast<uintptr_t>(ptr));
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::ol_i_id>>& orderline_value::get<NamedColumn::ol_i_id>() {
    return ol_i_id;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::ol_i_id>>& orderline_value::get<NamedColumn::ol_i_id>() const {
    return ol_i_id;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::ol_supply_w_id>>& orderline_value::get<NamedColumn::ol_supply_w_id>() {
    return ol_supply_w_id;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::ol_supply_w_id>>& orderline_value::get<NamedColumn::ol_supply_w_id>() const {
    return ol_supply_w_id;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::ol_quantity>>& orderline_value::get<NamedColumn::ol_quantity>() {
    return ol_quantity;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::ol_quantity>>& orderline_value::get<NamedColumn::ol_quantity>() const {
    return ol_quantity;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::ol_amount>>& orderline_value::get<NamedColumn::ol_amount>() {
    return ol_amount;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::ol_amount>>& orderline_value::get<NamedColumn::ol_amount>() const {
    return ol_amount;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::ol_dist_info>>& orderline_value::get<NamedColumn::ol_dist_info>() {
    return ol_dist_info;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::ol_dist_info>>& orderline_value::get<NamedColumn::ol_dist_info>() const {
    return ol_dist_info;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::ol_delivery_d>>& orderline_value::get<NamedColumn::ol_delivery_d>() {
    return ol_delivery_d;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::ol_delivery_d>>& orderline_value::get<NamedColumn::ol_delivery_d>() const {
    return ol_delivery_d;
}

};  // namespace orderline_value_datatypes

using orderline_value = orderline_value_datatypes::orderline_value;

namespace item_value_datatypes {

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

struct item_value;

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::i_im_id> {
    using NamedColumn = item_value_datatypes::NamedColumn;
    using struct_type = item_value;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::i_im_id;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::i_price> {
    using NamedColumn = item_value_datatypes::NamedColumn;
    using struct_type = item_value;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::i_price;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::i_name> {
    using NamedColumn = item_value_datatypes::NamedColumn;
    using struct_type = item_value;
    using type = var_string<24>;
    using value_type = var_string<24>;
    static constexpr NamedColumn Column = NamedColumn::i_name;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::i_data> {
    using NamedColumn = item_value_datatypes::NamedColumn;
    using struct_type = item_value;
    using type = var_string<50>;
    using value_type = var_string<50>;
    static constexpr NamedColumn Column = NamedColumn::i_data;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct item_value {
    using NamedColumn = item_value_datatypes::NamedColumn;
    static constexpr auto DEFAULT_SPLIT = NamedColumn::COLCOUNT;
    static constexpr auto MAX_SPLITS = 2;

    explicit item_value() = default;

    static inline void resplit(
            item_value& newvalue, const item_value& oldvalue, NamedColumn index);

    template <NamedColumn Column>
    static inline void copy_data(item_value& newvalue, const item_value& oldvalue);

    inline void init(const item_value* oldvalue = nullptr) {
        if (oldvalue) {
            auto split = oldvalue->splitindex_;
            if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Global)) {
                split = ADAPTER_OF(item_value)::CurrentSplit();
            }
            if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Inline)) {
                split = split;
            }
            item_value::resplit(*this, *oldvalue, split);
        }
    }

    template <NamedColumn Column>
    inline ::sto::adapter::Accessor<accessor_info<RoundedNamedColumn<Column>()>>& get();

    template <NamedColumn Column>
    inline const ::sto::adapter::Accessor<accessor_info<RoundedNamedColumn<Column>()>>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    ::sto::adapter::Accessor<accessor_info<NamedColumn::i_im_id>> i_im_id;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::i_price>> i_price;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::i_name>> i_name;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::i_data>> i_data;
    NamedColumn splitindex_ = DEFAULT_SPLIT;
};

inline void item_value::resplit(
        item_value& newvalue, const item_value& oldvalue, NamedColumn index) {
    assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
    if (&newvalue != &oldvalue) {
        memcpy(&newvalue, &oldvalue, sizeof newvalue);
        //copy_data<NamedColumn(0)>(newvalue, oldvalue);
    }
    newvalue.splitindex_ = index;
}

template <NamedColumn Column>
inline void item_value::copy_data(item_value& newvalue, const item_value& oldvalue) {
    static_assert(Column < NamedColumn::COLCOUNT);
    newvalue.template get<Column>().value_ = oldvalue.template get<Column>().value_;
    if constexpr (Column + 1 < NamedColumn::COLCOUNT) {
        copy_data<Column + 1>(newvalue, oldvalue);
    }
}

inline size_t accessor_info<NamedColumn::i_im_id>::offset() {
    item_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->i_im_id) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::i_price>::offset() {
    item_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->i_price) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::i_name>::offset() {
    item_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->i_name) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::i_data>::offset() {
    item_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->i_data) - reinterpret_cast<uintptr_t>(ptr));
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::i_im_id>>& item_value::get<NamedColumn::i_im_id>() {
    return i_im_id;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::i_im_id>>& item_value::get<NamedColumn::i_im_id>() const {
    return i_im_id;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::i_price>>& item_value::get<NamedColumn::i_price>() {
    return i_price;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::i_price>>& item_value::get<NamedColumn::i_price>() const {
    return i_price;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::i_name>>& item_value::get<NamedColumn::i_name>() {
    return i_name;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::i_name>>& item_value::get<NamedColumn::i_name>() const {
    return i_name;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::i_data>>& item_value::get<NamedColumn::i_data>() {
    return i_data;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::i_data>>& item_value::get<NamedColumn::i_data>() const {
    return i_data;
}

};  // namespace item_value_datatypes

using item_value = item_value_datatypes::item_value;

namespace stock_value_datatypes {

enum class NamedColumn : int {
    s_dists = 0,
    s_data = 10,
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

struct stock_value;

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::s_dists> {
    using NamedColumn = stock_value_datatypes::NamedColumn;
    using struct_type = stock_value;
    using type = fix_string<24>;
    using value_type = std::array<fix_string<24>, 10>;
    static constexpr NamedColumn Column = NamedColumn::s_dists;
    static constexpr bool is_array = true;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::s_data> {
    using NamedColumn = stock_value_datatypes::NamedColumn;
    using struct_type = stock_value;
    using type = var_string<50>;
    using value_type = var_string<50>;
    static constexpr NamedColumn Column = NamedColumn::s_data;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::s_quantity> {
    using NamedColumn = stock_value_datatypes::NamedColumn;
    using struct_type = stock_value;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::s_quantity;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::s_ytd> {
    using NamedColumn = stock_value_datatypes::NamedColumn;
    using struct_type = stock_value;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::s_ytd;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::s_order_cnt> {
    using NamedColumn = stock_value_datatypes::NamedColumn;
    using struct_type = stock_value;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::s_order_cnt;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::s_remote_cnt> {
    using NamedColumn = stock_value_datatypes::NamedColumn;
    using struct_type = stock_value;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::s_remote_cnt;
    static constexpr bool is_array = false;
    static inline size_t offset();
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct stock_value {
    using NamedColumn = stock_value_datatypes::NamedColumn;
    static constexpr auto DEFAULT_SPLIT = NamedColumn::COLCOUNT;
    static constexpr auto MAX_SPLITS = 2;

    explicit stock_value() = default;

    static inline void resplit(
            stock_value& newvalue, const stock_value& oldvalue, NamedColumn index);

    template <NamedColumn Column>
    static inline void copy_data(stock_value& newvalue, const stock_value& oldvalue);

    inline void init(const stock_value* oldvalue = nullptr) {
        if (oldvalue) {
            auto split = oldvalue->splitindex_;
            if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Global)) {
                split = ADAPTER_OF(stock_value)::CurrentSplit();
            }
            if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Inline)) {
                split = split;
            }
            stock_value::resplit(*this, *oldvalue, split);
        }
    }

    template <NamedColumn Column>
    inline ::sto::adapter::Accessor<accessor_info<RoundedNamedColumn<Column>()>>& get();

    template <NamedColumn Column>
    inline const ::sto::adapter::Accessor<accessor_info<RoundedNamedColumn<Column>()>>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    ::sto::adapter::Accessor<accessor_info<NamedColumn::s_dists>> s_dists;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::s_data>> s_data;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::s_quantity>> s_quantity;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::s_ytd>> s_ytd;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::s_order_cnt>> s_order_cnt;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::s_remote_cnt>> s_remote_cnt;
    NamedColumn splitindex_ = DEFAULT_SPLIT;
};

inline void stock_value::resplit(
        stock_value& newvalue, const stock_value& oldvalue, NamedColumn index) {
    assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
    if (&newvalue != &oldvalue) {
        memcpy(&newvalue, &oldvalue, sizeof newvalue);
        //copy_data<NamedColumn(0)>(newvalue, oldvalue);
    }
    newvalue.splitindex_ = index;
}

template <NamedColumn Column>
inline void stock_value::copy_data(stock_value& newvalue, const stock_value& oldvalue) {
    static_assert(Column < NamedColumn::COLCOUNT);
    newvalue.template get<Column>().value_ = oldvalue.template get<Column>().value_;
    if constexpr (Column + 1 < NamedColumn::COLCOUNT) {
        copy_data<Column + 1>(newvalue, oldvalue);
    }
}

inline size_t accessor_info<NamedColumn::s_dists>::offset() {
    stock_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->s_dists) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::s_data>::offset() {
    stock_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->s_data) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::s_quantity>::offset() {
    stock_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->s_quantity) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::s_ytd>::offset() {
    stock_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->s_ytd) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::s_order_cnt>::offset() {
    stock_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->s_order_cnt) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::s_remote_cnt>::offset() {
    stock_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->s_remote_cnt) - reinterpret_cast<uintptr_t>(ptr));
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::s_dists>>& stock_value::get<NamedColumn::s_dists + 0>() {
    return s_dists;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::s_dists>>& stock_value::get<NamedColumn::s_dists + 0>() const {
    return s_dists;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::s_dists>>& stock_value::get<NamedColumn::s_dists + 1>() {
    return s_dists;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::s_dists>>& stock_value::get<NamedColumn::s_dists + 1>() const {
    return s_dists;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::s_dists>>& stock_value::get<NamedColumn::s_dists + 2>() {
    return s_dists;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::s_dists>>& stock_value::get<NamedColumn::s_dists + 2>() const {
    return s_dists;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::s_dists>>& stock_value::get<NamedColumn::s_dists + 3>() {
    return s_dists;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::s_dists>>& stock_value::get<NamedColumn::s_dists + 3>() const {
    return s_dists;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::s_dists>>& stock_value::get<NamedColumn::s_dists + 4>() {
    return s_dists;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::s_dists>>& stock_value::get<NamedColumn::s_dists + 4>() const {
    return s_dists;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::s_dists>>& stock_value::get<NamedColumn::s_dists + 5>() {
    return s_dists;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::s_dists>>& stock_value::get<NamedColumn::s_dists + 5>() const {
    return s_dists;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::s_dists>>& stock_value::get<NamedColumn::s_dists + 6>() {
    return s_dists;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::s_dists>>& stock_value::get<NamedColumn::s_dists + 6>() const {
    return s_dists;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::s_dists>>& stock_value::get<NamedColumn::s_dists + 7>() {
    return s_dists;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::s_dists>>& stock_value::get<NamedColumn::s_dists + 7>() const {
    return s_dists;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::s_dists>>& stock_value::get<NamedColumn::s_dists + 8>() {
    return s_dists;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::s_dists>>& stock_value::get<NamedColumn::s_dists + 8>() const {
    return s_dists;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::s_dists>>& stock_value::get<NamedColumn::s_dists + 9>() {
    return s_dists;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::s_dists>>& stock_value::get<NamedColumn::s_dists + 9>() const {
    return s_dists;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::s_data>>& stock_value::get<NamedColumn::s_data>() {
    return s_data;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::s_data>>& stock_value::get<NamedColumn::s_data>() const {
    return s_data;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::s_quantity>>& stock_value::get<NamedColumn::s_quantity>() {
    return s_quantity;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::s_quantity>>& stock_value::get<NamedColumn::s_quantity>() const {
    return s_quantity;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::s_ytd>>& stock_value::get<NamedColumn::s_ytd>() {
    return s_ytd;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::s_ytd>>& stock_value::get<NamedColumn::s_ytd>() const {
    return s_ytd;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::s_order_cnt>>& stock_value::get<NamedColumn::s_order_cnt>() {
    return s_order_cnt;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::s_order_cnt>>& stock_value::get<NamedColumn::s_order_cnt>() const {
    return s_order_cnt;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::s_remote_cnt>>& stock_value::get<NamedColumn::s_remote_cnt>() {
    return s_remote_cnt;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::s_remote_cnt>>& stock_value::get<NamedColumn::s_remote_cnt>() const {
    return s_remote_cnt;
}

};  // namespace stock_value_datatypes

using stock_value = stock_value_datatypes::stock_value;

