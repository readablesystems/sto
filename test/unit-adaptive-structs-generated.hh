#pragma once

#include <type_traits>

#include "Adapter.hh"
#include "Sto.hh"

namespace index_value_datatypes {

enum class NamedColumn : int {
    data = 0,
    label = 2,
    flagged,
    COLCOUNT
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
    if constexpr (Column < NamedColumn::label) {
        return NamedColumn::data;
    }
    if constexpr (Column < NamedColumn::flagged) {
        return NamedColumn::label;
    }
    return NamedColumn::flagged;
}

template <NamedColumn Column>
struct accessor;

struct index_value;

DEFINE_ADAPTER(index_value, NamedColumn);

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::data> {
    using type = double;
    using value_type = std::array<double, 2>;
    static constexpr bool is_array = true;
};

template <>
struct accessor_info<NamedColumn::label> {
    using type = std::string;
    using value_type = std::string;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::flagged> {
    using type = bool;
    using value_type = bool;
    static constexpr bool is_array = false;
};

template <NamedColumn Column>
struct accessor {
    using adapter_type = ADAPTER_OF(index_value);
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

struct index_value {
    using NamedColumn = index_value_datatypes::NamedColumn;
    static constexpr auto MAX_SPLITS = 2;

    explicit index_value() = default;

    static inline void resplit(
            index_value& newvalue, const index_value& oldvalue, NamedColumn index);

    template <NamedColumn Column>
    static inline void copy_data(index_value& newvalue, const index_value& oldvalue);

    template <NamedColumn Column>
    inline accessor<RoundedNamedColumn<Column>()>& get();

    template <NamedColumn Column>
    inline const accessor<RoundedNamedColumn<Column>()>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    NamedColumn splitindex_ = NamedColumn::COLCOUNT;
    accessor<NamedColumn::data> data;
    accessor<NamedColumn::label> label;
    accessor<NamedColumn::flagged> flagged;
};

inline void index_value::resplit(
        index_value& newvalue, const index_value& oldvalue, NamedColumn index) {
    assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
    memcpy(&newvalue, &oldvalue, sizeof newvalue);
    //copy_data<NamedColumn(0)>(newvalue, oldvalue);
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

template <>
inline accessor<NamedColumn::data>& index_value::get<NamedColumn::data + 0>() {
    return data;
}

template <>
inline const accessor<NamedColumn::data>& index_value::get<NamedColumn::data + 0>() const {
    return data;
}

template <>
inline accessor<NamedColumn::data>& index_value::get<NamedColumn::data + 1>() {
    return data;
}

template <>
inline const accessor<NamedColumn::data>& index_value::get<NamedColumn::data + 1>() const {
    return data;
}

template <>
inline accessor<NamedColumn::label>& index_value::get<NamedColumn::label>() {
    return label;
}

template <>
inline const accessor<NamedColumn::label>& index_value::get<NamedColumn::label>() const {
    return label;
}

template <>
inline accessor<NamedColumn::flagged>& index_value::get<NamedColumn::flagged>() {
    return flagged;
}

template <>
inline const accessor<NamedColumn::flagged>& index_value::get<NamedColumn::flagged>() const {
    return flagged;
}

};  // namespace index_value_datatypes

using index_value = index_value_datatypes::index_value;
using ADAPTER_OF(index_value) = ADAPTER_OF(index_value_datatypes::index_value);

