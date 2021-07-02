#pragma once

#include <type_traits>

#include "Adapter.hh"
#include "Sto.hh"

namespace ordered_value_datatypes {

enum class NamedColumn : int {
    ro = 0,
    rw = 2,
    wo = 10,
    COLCOUNT = 12
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
    if constexpr (Column < NamedColumn::rw) {
        return NamedColumn::ro;
    }
    if constexpr (Column < NamedColumn::wo) {
        return NamedColumn::rw;
    }
    return NamedColumn::wo;
}

template <NamedColumn Column>
struct accessor;

struct ordered_value;

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::ro> {
    using type = uint64_t;
    using value_type = std::array<uint64_t, 2>;
    static constexpr bool is_array = true;
};

template <>
struct accessor_info<NamedColumn::rw> {
    using type = uint64_t;
    using value_type = std::array<uint64_t, 8>;
    static constexpr bool is_array = true;
};

template <>
struct accessor_info<NamedColumn::wo> {
    using type = uint64_t;
    using value_type = std::array<uint64_t, 2>;
    static constexpr bool is_array = true;
};

template <NamedColumn Column>
struct accessor {
    using adapter_type = ::sto::GlobalAdapter<ordered_value, NamedColumn>;
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

struct ordered_value {
    using NamedColumn = ordered_value_datatypes::NamedColumn;
    static constexpr auto DEFAULT_SPLIT = NamedColumn::rw + 4;
    static constexpr auto MAX_SPLITS = 2;

    explicit ordered_value() = default;

    static inline void resplit(
            ordered_value& newvalue, const ordered_value& oldvalue, NamedColumn index);

    template <NamedColumn Column>
    static inline void copy_data(ordered_value& newvalue, const ordered_value& oldvalue);

    inline void init(const ordered_value* oldvalue = nullptr) {
        if (oldvalue) {
            ordered_value::resplit(*this, *oldvalue, ADAPTER_OF(ordered_value)::CurrentSplit());
        }
    }

    template <NamedColumn Column>
    inline accessor<RoundedNamedColumn<Column>()>& get();

    template <NamedColumn Column>
    inline const accessor<RoundedNamedColumn<Column>()>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    NamedColumn splitindex_ = DEFAULT_SPLIT;
    accessor<NamedColumn::ro> ro;
    accessor<NamedColumn::rw> rw;
    accessor<NamedColumn::wo> wo;
};

inline void ordered_value::resplit(
        ordered_value& newvalue, const ordered_value& oldvalue, NamedColumn index) {
    assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
    if (&newvalue != &oldvalue) {
        memcpy(&newvalue, &oldvalue, sizeof newvalue);
        //copy_data<NamedColumn(0)>(newvalue, oldvalue);
    }
    newvalue.splitindex_ = index;
}

template <NamedColumn Column>
inline void ordered_value::copy_data(ordered_value& newvalue, const ordered_value& oldvalue) {
    static_assert(Column < NamedColumn::COLCOUNT);
    newvalue.template get<Column>().value_ = oldvalue.template get<Column>().value_;
    if constexpr (Column + 1 < NamedColumn::COLCOUNT) {
        copy_data<Column + 1>(newvalue, oldvalue);
    }
}

template <>
inline accessor<NamedColumn::ro>& ordered_value::get<NamedColumn::ro + 0>() {
    return ro;
}

template <>
inline const accessor<NamedColumn::ro>& ordered_value::get<NamedColumn::ro + 0>() const {
    return ro;
}

template <>
inline accessor<NamedColumn::ro>& ordered_value::get<NamedColumn::ro + 1>() {
    return ro;
}

template <>
inline const accessor<NamedColumn::ro>& ordered_value::get<NamedColumn::ro + 1>() const {
    return ro;
}

template <>
inline accessor<NamedColumn::rw>& ordered_value::get<NamedColumn::rw + 0>() {
    return rw;
}

template <>
inline const accessor<NamedColumn::rw>& ordered_value::get<NamedColumn::rw + 0>() const {
    return rw;
}

template <>
inline accessor<NamedColumn::rw>& ordered_value::get<NamedColumn::rw + 1>() {
    return rw;
}

template <>
inline const accessor<NamedColumn::rw>& ordered_value::get<NamedColumn::rw + 1>() const {
    return rw;
}

template <>
inline accessor<NamedColumn::rw>& ordered_value::get<NamedColumn::rw + 2>() {
    return rw;
}

template <>
inline const accessor<NamedColumn::rw>& ordered_value::get<NamedColumn::rw + 2>() const {
    return rw;
}

template <>
inline accessor<NamedColumn::rw>& ordered_value::get<NamedColumn::rw + 3>() {
    return rw;
}

template <>
inline const accessor<NamedColumn::rw>& ordered_value::get<NamedColumn::rw + 3>() const {
    return rw;
}

template <>
inline accessor<NamedColumn::rw>& ordered_value::get<NamedColumn::rw + 4>() {
    return rw;
}

template <>
inline const accessor<NamedColumn::rw>& ordered_value::get<NamedColumn::rw + 4>() const {
    return rw;
}

template <>
inline accessor<NamedColumn::rw>& ordered_value::get<NamedColumn::rw + 5>() {
    return rw;
}

template <>
inline const accessor<NamedColumn::rw>& ordered_value::get<NamedColumn::rw + 5>() const {
    return rw;
}

template <>
inline accessor<NamedColumn::rw>& ordered_value::get<NamedColumn::rw + 6>() {
    return rw;
}

template <>
inline const accessor<NamedColumn::rw>& ordered_value::get<NamedColumn::rw + 6>() const {
    return rw;
}

template <>
inline accessor<NamedColumn::rw>& ordered_value::get<NamedColumn::rw + 7>() {
    return rw;
}

template <>
inline const accessor<NamedColumn::rw>& ordered_value::get<NamedColumn::rw + 7>() const {
    return rw;
}

template <>
inline accessor<NamedColumn::wo>& ordered_value::get<NamedColumn::wo + 0>() {
    return wo;
}

template <>
inline const accessor<NamedColumn::wo>& ordered_value::get<NamedColumn::wo + 0>() const {
    return wo;
}

template <>
inline accessor<NamedColumn::wo>& ordered_value::get<NamedColumn::wo + 1>() {
    return wo;
}

template <>
inline const accessor<NamedColumn::wo>& ordered_value::get<NamedColumn::wo + 1>() const {
    return wo;
}

};  // namespace ordered_value_datatypes

using ordered_value = ordered_value_datatypes::ordered_value;

namespace unordered_value_datatypes {

enum class NamedColumn : int {
    ro = 0,
    rw = 2,
    wo = 10,
    COLCOUNT = 12
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
    if constexpr (Column < NamedColumn::rw) {
        return NamedColumn::ro;
    }
    if constexpr (Column < NamedColumn::wo) {
        return NamedColumn::rw;
    }
    return NamedColumn::wo;
}

template <NamedColumn Column>
struct accessor;

struct unordered_value;

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::ro> {
    using type = uint64_t;
    using value_type = std::array<uint64_t, 2>;
    static constexpr bool is_array = true;
};

template <>
struct accessor_info<NamedColumn::rw> {
    using type = uint64_t;
    using value_type = std::array<uint64_t, 8>;
    static constexpr bool is_array = true;
};

template <>
struct accessor_info<NamedColumn::wo> {
    using type = uint64_t;
    using value_type = std::array<uint64_t, 2>;
    static constexpr bool is_array = true;
};

template <NamedColumn Column>
struct accessor {
    using adapter_type = ::sto::GlobalAdapter<unordered_value, NamedColumn>;
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

struct unordered_value {
    using NamedColumn = unordered_value_datatypes::NamedColumn;
    static constexpr auto DEFAULT_SPLIT = NamedColumn::rw + 4;
    static constexpr auto MAX_SPLITS = 2;

    explicit unordered_value() = default;

    static inline void resplit(
            unordered_value& newvalue, const unordered_value& oldvalue, NamedColumn index);

    template <NamedColumn Column>
    static inline void copy_data(unordered_value& newvalue, const unordered_value& oldvalue);

    inline void init(const unordered_value* oldvalue = nullptr) {
        if (oldvalue) {
            unordered_value::resplit(*this, *oldvalue, ADAPTER_OF(unordered_value)::CurrentSplit());
        }
    }

    template <NamedColumn Column>
    inline accessor<RoundedNamedColumn<Column>()>& get();

    template <NamedColumn Column>
    inline const accessor<RoundedNamedColumn<Column>()>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    NamedColumn splitindex_ = DEFAULT_SPLIT;
    accessor<NamedColumn::ro> ro;
    accessor<NamedColumn::rw> rw;
    accessor<NamedColumn::wo> wo;
};

inline void unordered_value::resplit(
        unordered_value& newvalue, const unordered_value& oldvalue, NamedColumn index) {
    assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
    if (&newvalue != &oldvalue) {
        memcpy(&newvalue, &oldvalue, sizeof newvalue);
        //copy_data<NamedColumn(0)>(newvalue, oldvalue);
    }
    newvalue.splitindex_ = index;
}

template <NamedColumn Column>
inline void unordered_value::copy_data(unordered_value& newvalue, const unordered_value& oldvalue) {
    static_assert(Column < NamedColumn::COLCOUNT);
    newvalue.template get<Column>().value_ = oldvalue.template get<Column>().value_;
    if constexpr (Column + 1 < NamedColumn::COLCOUNT) {
        copy_data<Column + 1>(newvalue, oldvalue);
    }
}

template <>
inline accessor<NamedColumn::ro>& unordered_value::get<NamedColumn::ro + 0>() {
    return ro;
}

template <>
inline const accessor<NamedColumn::ro>& unordered_value::get<NamedColumn::ro + 0>() const {
    return ro;
}

template <>
inline accessor<NamedColumn::ro>& unordered_value::get<NamedColumn::ro + 1>() {
    return ro;
}

template <>
inline const accessor<NamedColumn::ro>& unordered_value::get<NamedColumn::ro + 1>() const {
    return ro;
}

template <>
inline accessor<NamedColumn::rw>& unordered_value::get<NamedColumn::rw + 0>() {
    return rw;
}

template <>
inline const accessor<NamedColumn::rw>& unordered_value::get<NamedColumn::rw + 0>() const {
    return rw;
}

template <>
inline accessor<NamedColumn::rw>& unordered_value::get<NamedColumn::rw + 1>() {
    return rw;
}

template <>
inline const accessor<NamedColumn::rw>& unordered_value::get<NamedColumn::rw + 1>() const {
    return rw;
}

template <>
inline accessor<NamedColumn::rw>& unordered_value::get<NamedColumn::rw + 2>() {
    return rw;
}

template <>
inline const accessor<NamedColumn::rw>& unordered_value::get<NamedColumn::rw + 2>() const {
    return rw;
}

template <>
inline accessor<NamedColumn::rw>& unordered_value::get<NamedColumn::rw + 3>() {
    return rw;
}

template <>
inline const accessor<NamedColumn::rw>& unordered_value::get<NamedColumn::rw + 3>() const {
    return rw;
}

template <>
inline accessor<NamedColumn::rw>& unordered_value::get<NamedColumn::rw + 4>() {
    return rw;
}

template <>
inline const accessor<NamedColumn::rw>& unordered_value::get<NamedColumn::rw + 4>() const {
    return rw;
}

template <>
inline accessor<NamedColumn::rw>& unordered_value::get<NamedColumn::rw + 5>() {
    return rw;
}

template <>
inline const accessor<NamedColumn::rw>& unordered_value::get<NamedColumn::rw + 5>() const {
    return rw;
}

template <>
inline accessor<NamedColumn::rw>& unordered_value::get<NamedColumn::rw + 6>() {
    return rw;
}

template <>
inline const accessor<NamedColumn::rw>& unordered_value::get<NamedColumn::rw + 6>() const {
    return rw;
}

template <>
inline accessor<NamedColumn::rw>& unordered_value::get<NamedColumn::rw + 7>() {
    return rw;
}

template <>
inline const accessor<NamedColumn::rw>& unordered_value::get<NamedColumn::rw + 7>() const {
    return rw;
}

template <>
inline accessor<NamedColumn::wo>& unordered_value::get<NamedColumn::wo + 0>() {
    return wo;
}

template <>
inline const accessor<NamedColumn::wo>& unordered_value::get<NamedColumn::wo + 0>() const {
    return wo;
}

template <>
inline accessor<NamedColumn::wo>& unordered_value::get<NamedColumn::wo + 1>() {
    return wo;
}

template <>
inline const accessor<NamedColumn::wo>& unordered_value::get<NamedColumn::wo + 1>() const {
    return wo;
}

};  // namespace unordered_value_datatypes

using unordered_value = unordered_value_datatypes::unordered_value;

