#pragma once

#include <type_traits>

#include "Adapter.hh"
#include "Sto.hh"

namespace ycsb {

namespace ycsb_value_datatypes {

enum class NamedColumn : int {
    even_columns = 0,
    odd_columns = 5,
    COLCOUNT = 10
};

constexpr NamedColumn operator+(NamedColumn nc, std::underlying_type_t<NamedColumn> index) {
    return NamedColumn(static_cast<std::underlying_type_t<NamedColumn>>(nc) + index);
}

NamedColumn& operator+=(NamedColumn& nc, std::underlying_type_t<NamedColumn> index) {
    nc = static_cast<NamedColumn>(static_cast<std::underlying_type_t<NamedColumn>>(nc) + index);
    return nc;
}

NamedColumn& operator++(NamedColumn& nc) {
    return nc += 1;
}

NamedColumn& operator++(NamedColumn& nc, int) {
    return nc += 1;
}

constexpr NamedColumn operator-(NamedColumn nc, std::underlying_type_t<NamedColumn> index) {
    return NamedColumn(static_cast<std::underlying_type_t<NamedColumn>>(nc) - index);
}

NamedColumn& operator-=(NamedColumn& nc, std::underlying_type_t<NamedColumn> index) {
    nc = static_cast<NamedColumn>(static_cast<std::underlying_type_t<NamedColumn>>(nc) - index);
    return nc;
}

std::ostream& operator<<(std::ostream& out, NamedColumn& nc) {
    out << static_cast<std::underlying_type_t<NamedColumn>>(nc);
    return out;
}

template <NamedColumn Column>
constexpr NamedColumn RoundedNamedColumn() {
    static_assert(Column < NamedColumn::COLCOUNT);
    if constexpr (Column < NamedColumn::odd_columns) {
        return NamedColumn::even_columns;
    }
    return NamedColumn::odd_columns;
}

template <NamedColumn Column>
struct accessor;

struct ycsb_value;

DEFINE_ADAPTER(ycsb_value, NamedColumn);

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::even_columns> {
    using type = ::bench::fix_string<COL_WIDTH>;
    using value_type = std::array<::bench::fix_string<COL_WIDTH>, 5>;
    static constexpr bool is_array = true;
};

template <>
struct accessor_info<NamedColumn::odd_columns> {
    using type = ::bench::fix_string<COL_WIDTH>;
    using value_type = std::array<::bench::fix_string<COL_WIDTH>, 5>;
    static constexpr bool is_array = true;
};

template <NamedColumn Column>
struct accessor {
    using adapter_type = ADAPTER_OF(ycsb_value);
    using type = typename accessor_info<Column>::type;
    using value_type = typename accessor_info<Column>::value_type;

    accessor() = default;
    template <typename... Args>
    explicit accessor(Args&&... args) : value_(std::forward<Args>(args)...) {}

    operator const value_type() const {
        if constexpr (accessor_info<Column>::is_array) {
            adapter_type::CountReads(Column, Column + value_.size());
        } else {
            adapter_type::CountRead(Column);
        }
        return value_;
    }

    value_type operator =(const value_type& other) {
        if constexpr (accessor_info<Column>::is_array) {
            adapter_type::CountWrites(Column, Column + value_.size());
        } else {
            adapter_type::CountWrite(Column);
        }
        return value_ = other;
    }

    value_type operator =(accessor<Column>& other) {
        if constexpr (accessor_info<Column>::is_array) {
            adapter_type::CountReads(Column, Column + other.value_.size());
            adapter_type::CountWrites(Column, Column + value_.size());
        } else {
            adapter_type::CountRead(Column);
            adapter_type::CountWrite(Column);
        }
        return value_ = other.value_;
    }

    template <NamedColumn OtherColumn>
    value_type operator =(const accessor<OtherColumn>& other) {
        if constexpr (accessor_info<Column>::is_array && accessor_info<OtherColumn>::is_array) {
            adapter_type::CountReads(OtherColumn, OtherColumn + other.value_.size());
            adapter_type::CountWrites(Column, Column + value_.size());
        } else {
            adapter_type::CountRead(OtherColumn);
            adapter_type::CountWrite(Column);
        }
        return value_ = other.value_;
    }

    template <bool is_array = accessor_info<Column>::is_array>
    std::enable_if_t<is_array, void>
    operator ()(const std::underlying_type_t<NamedColumn>& index, const type& value) {
        adapter_type::CountWrite(Column + index);
        value_[index] = value;
    }

    template <bool is_array = accessor_info<Column>::is_array>
    std::enable_if_t<is_array, type&>
    operator ()(const std::underlying_type_t<NamedColumn>& index) {
        adapter_type::CountWrite(Column + index);
        return value_[index];
    }

    template <bool is_array = accessor_info<Column>::is_array>
    std::enable_if_t<is_array, type>
    operator [](const std::underlying_type_t<NamedColumn>& index) {
        adapter_type::CountRead(Column + index);
        return value_[index];
    }

    template <bool is_array = accessor_info<Column>::is_array>
    std::enable_if_t<is_array, const type&>
    operator [](const std::underlying_type_t<NamedColumn>& index) const {
        adapter_type::CountRead(Column + index);
        return value_[index];
    }

    template <bool is_array = accessor_info<Column>::is_array>
    std::enable_if_t<!is_array, type&>
    operator *() {
        adapter_type::CountWrite(Column);
        return value_;
    }

    template <bool is_array = accessor_info<Column>::is_array>
    std::enable_if_t<!is_array, type*>
    operator ->() {
        adapter_type::CountWrite(Column);
        return &value_;
    }

    value_type value_;
};

struct ycsb_value {
    using NamedColumn = ycsb_value_datatypes::NamedColumn;
    static constexpr auto MAX_SPLITS = 2;

    explicit ycsb_value() = default;

    static inline void resplit(
            ycsb_value& newvalue, const ycsb_value& oldvalue, NamedColumn index);

    template <NamedColumn Column>
    static inline void copy_data(ycsb_value& newvalue, const ycsb_value& oldvalue);

    template <NamedColumn Column>
    inline accessor<RoundedNamedColumn<Column>()>& get();

    template <NamedColumn Column>
    inline const accessor<RoundedNamedColumn<Column>()>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    NamedColumn splitindex_ = NamedColumn::COLCOUNT;
    accessor<NamedColumn::even_columns> even_columns;
    accessor<NamedColumn::odd_columns> odd_columns;
};

inline void ycsb_value::resplit(
        ycsb_value& newvalue, const ycsb_value& oldvalue, NamedColumn index) {
    assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
    memcpy(&newvalue, &oldvalue, sizeof newvalue);
    //copy_data<NamedColumn(0)>(newvalue, oldvalue);
    newvalue.splitindex_ = index;
}

template <NamedColumn Column>
inline void ycsb_value::copy_data(ycsb_value& newvalue, const ycsb_value& oldvalue) {
    static_assert(Column < NamedColumn::COLCOUNT);
    newvalue.template get<Column>().value_ = oldvalue.template get<Column>().value_;
    if constexpr (Column + 1 < NamedColumn::COLCOUNT) {
        copy_data<Column + 1>(newvalue, oldvalue);
    }
}

template <>
inline accessor<NamedColumn::even_columns>& ycsb_value::get<NamedColumn::even_columns + 0>() {
    return even_columns;
}

template <>
inline const accessor<NamedColumn::even_columns>& ycsb_value::get<NamedColumn::even_columns + 0>() const {
    return even_columns;
}

template <>
inline accessor<NamedColumn::even_columns>& ycsb_value::get<NamedColumn::even_columns + 1>() {
    return even_columns;
}

template <>
inline const accessor<NamedColumn::even_columns>& ycsb_value::get<NamedColumn::even_columns + 1>() const {
    return even_columns;
}

template <>
inline accessor<NamedColumn::even_columns>& ycsb_value::get<NamedColumn::even_columns + 2>() {
    return even_columns;
}

template <>
inline const accessor<NamedColumn::even_columns>& ycsb_value::get<NamedColumn::even_columns + 2>() const {
    return even_columns;
}

template <>
inline accessor<NamedColumn::even_columns>& ycsb_value::get<NamedColumn::even_columns + 3>() {
    return even_columns;
}

template <>
inline const accessor<NamedColumn::even_columns>& ycsb_value::get<NamedColumn::even_columns + 3>() const {
    return even_columns;
}

template <>
inline accessor<NamedColumn::even_columns>& ycsb_value::get<NamedColumn::even_columns + 4>() {
    return even_columns;
}

template <>
inline const accessor<NamedColumn::even_columns>& ycsb_value::get<NamedColumn::even_columns + 4>() const {
    return even_columns;
}

template <>
inline accessor<NamedColumn::odd_columns>& ycsb_value::get<NamedColumn::odd_columns + 0>() {
    return odd_columns;
}

template <>
inline const accessor<NamedColumn::odd_columns>& ycsb_value::get<NamedColumn::odd_columns + 0>() const {
    return odd_columns;
}

template <>
inline accessor<NamedColumn::odd_columns>& ycsb_value::get<NamedColumn::odd_columns + 1>() {
    return odd_columns;
}

template <>
inline const accessor<NamedColumn::odd_columns>& ycsb_value::get<NamedColumn::odd_columns + 1>() const {
    return odd_columns;
}

template <>
inline accessor<NamedColumn::odd_columns>& ycsb_value::get<NamedColumn::odd_columns + 2>() {
    return odd_columns;
}

template <>
inline const accessor<NamedColumn::odd_columns>& ycsb_value::get<NamedColumn::odd_columns + 2>() const {
    return odd_columns;
}

template <>
inline accessor<NamedColumn::odd_columns>& ycsb_value::get<NamedColumn::odd_columns + 3>() {
    return odd_columns;
}

template <>
inline const accessor<NamedColumn::odd_columns>& ycsb_value::get<NamedColumn::odd_columns + 3>() const {
    return odd_columns;
}

template <>
inline accessor<NamedColumn::odd_columns>& ycsb_value::get<NamedColumn::odd_columns + 4>() {
    return odd_columns;
}

template <>
inline const accessor<NamedColumn::odd_columns>& ycsb_value::get<NamedColumn::odd_columns + 4>() const {
    return odd_columns;
}

};  // namespace ycsb_value_datatypes

using ycsb_value = ycsb_value_datatypes::ycsb_value;
using ADAPTER_OF(ycsb_value) = ADAPTER_OF(ycsb_value_datatypes::ycsb_value);

};  // namespace ycsb

