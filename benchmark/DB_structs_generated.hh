#pragma once

#include <type_traits>

namespace dummy_row_datatypes {

enum class NamedColumn : int {
    dummy = 0,
    COLCOUNT
};

inline constexpr NamedColumn operator+(NamedColumn nc, std::underlying_type_t<NamedColumn> index) {
    return NamedColumn(static_cast<std::underlying_type_t<NamedColumn>>(nc) + index);
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

inline NamedColumn& operator-=(NamedColumn& nc, std::underlying_type_t<NamedColumn> index) {
    nc = static_cast<NamedColumn>(static_cast<std::underlying_type_t<NamedColumn>>(nc) - index);
    return nc;
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

template <NamedColumn Column>
struct accessor;

struct dummy_row;

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::dummy> {
    using type = uintptr_t;
    using value_type = uintptr_t;
    static constexpr bool is_array = false;
};

template <NamedColumn Column>
struct accessor {
    using type = typename accessor_info<Column>::type;
    using value_type = typename accessor_info<Column>::value_type;

    accessor() = default;
    template <typename... Args>
    explicit accessor(Args&&... args) : value_(std::forward<Args>(args)...) {}

    operator const value_type() const {
        return value_;
    }

    value_type operator =(const value_type& other) {
        return value_ = other;
    }

    value_type operator =(accessor<Column>& other) {
        return value_ = other.value_;
    }

    template <NamedColumn OtherColumn>
    value_type operator =(const accessor<OtherColumn>& other) {
        return value_ = other.value_;
    }

    template <bool is_array = accessor_info<Column>::is_array>
    std::enable_if_t<is_array, void>
    operator ()(const std::underlying_type_t<NamedColumn>& index, const type& value) {
        value_[index] = value;
    }

    template <bool is_array = accessor_info<Column>::is_array>
    std::enable_if_t<is_array, type&>
    operator ()(const std::underlying_type_t<NamedColumn>& index) {
        return value_[index];
    }

    template <bool is_array = accessor_info<Column>::is_array>
    std::enable_if_t<is_array, type>
    operator [](const std::underlying_type_t<NamedColumn>& index) {
        return value_[index];
    }

    template <bool is_array = accessor_info<Column>::is_array>
    std::enable_if_t<is_array, const type&>
    operator [](const std::underlying_type_t<NamedColumn>& index) const {
        return value_[index];
    }

    template <bool is_array = accessor_info<Column>::is_array>
    std::enable_if_t<!is_array, type&>
    operator *() {
        return value_;
    }

    template <bool is_array = accessor_info<Column>::is_array>
    std::enable_if_t<!is_array, type*>
    operator ->() {
        return &value_;
    }

    value_type value_;
};

struct dummy_row {
    using NamedColumn = dummy_row_datatypes::NamedColumn;
    static constexpr auto MAX_SPLITS = 2;
    static dummy_row row;

    dummy_row() = default;

    static inline void resplit(
            dummy_row& newvalue, const dummy_row& oldvalue, NamedColumn index);

    template <NamedColumn Column>
    static inline void copy_data(dummy_row& newvalue, const dummy_row& oldvalue);

    template <NamedColumn Column>
    inline accessor<RoundedNamedColumn<Column>()>& get();

    template <NamedColumn Column>
    inline const accessor<RoundedNamedColumn<Column>()>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    NamedColumn splitindex_ = NamedColumn::COLCOUNT;
    accessor<NamedColumn::dummy> dummy;
};

inline void dummy_row::resplit(
        dummy_row& newvalue, const dummy_row& oldvalue, NamedColumn index) {
    assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
    memcpy(&newvalue, &oldvalue, sizeof newvalue);
    //copy_data<NamedColumn(0)>(newvalue, oldvalue);
    newvalue.splitindex_ = index;
}

template <NamedColumn Column>
inline void dummy_row::copy_data(dummy_row& newvalue, const dummy_row& oldvalue) {
    static_assert(Column < NamedColumn::COLCOUNT);
    newvalue.template get<Column>().value_ = oldvalue.template get<Column>().value_;
    if constexpr (Column + 1 < NamedColumn::COLCOUNT) {
        copy_data<Column + 1>(newvalue, oldvalue);
    }
}

template <>
inline accessor<NamedColumn::dummy>& dummy_row::get<NamedColumn::dummy>() {
    return dummy;
}

template <>
inline const accessor<NamedColumn::dummy>& dummy_row::get<NamedColumn::dummy>() const {
    return dummy;
}

};  // namespace dummy_row_datatypes

using dummy_row = dummy_row_datatypes::dummy_row;

