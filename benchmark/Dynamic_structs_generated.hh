#pragma once

#include <type_traits>

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

struct ordered_value;

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::ro> {
    using NamedColumn = ordered_value_datatypes::NamedColumn;
    using struct_type = ordered_value;
    using type = uint64_t;
    using value_type = std::array<uint64_t, 2>;
    static constexpr NamedColumn Column = NamedColumn::ro;
    static constexpr bool is_array = true;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::rw> {
    using NamedColumn = ordered_value_datatypes::NamedColumn;
    using struct_type = ordered_value;
    using type = uint64_t;
    using value_type = std::array<uint64_t, 8>;
    static constexpr NamedColumn Column = NamedColumn::rw;
    static constexpr bool is_array = true;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::wo> {
    using NamedColumn = ordered_value_datatypes::NamedColumn;
    using struct_type = ordered_value;
    using type = uint64_t;
    using value_type = std::array<uint64_t, 2>;
    static constexpr NamedColumn Column = NamedColumn::wo;
    static constexpr bool is_array = true;
    static inline size_t offset();
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
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
            auto split = oldvalue->splitindex_;
            if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Global)) {
                split = ADAPTER_OF(ordered_value)::CurrentSplit();
            }
            if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Inline)) {
                split = split;
            }
            ordered_value::resplit(*this, *oldvalue, split);
        }
    }

    template <NamedColumn Column>
    inline ::sto::adapter::Accessor<accessor_info<RoundedNamedColumn<Column>()>>& get();

    template <NamedColumn Column>
    inline const ::sto::adapter::Accessor<accessor_info<RoundedNamedColumn<Column>()>>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    ::sto::adapter::Accessor<accessor_info<NamedColumn::ro>> ro;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>> rw;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::wo>> wo;
    NamedColumn splitindex_ = DEFAULT_SPLIT;
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

inline size_t accessor_info<NamedColumn::ro>::offset() {
    ordered_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->ro) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::rw>::offset() {
    ordered_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->rw) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::wo>::offset() {
    ordered_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->wo) - reinterpret_cast<uintptr_t>(ptr));
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::ro>>& ordered_value::get<NamedColumn::ro + 0>() {
    return ro;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::ro>>& ordered_value::get<NamedColumn::ro + 0>() const {
    return ro;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::ro>>& ordered_value::get<NamedColumn::ro + 1>() {
    return ro;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::ro>>& ordered_value::get<NamedColumn::ro + 1>() const {
    return ro;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& ordered_value::get<NamedColumn::rw + 0>() {
    return rw;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& ordered_value::get<NamedColumn::rw + 0>() const {
    return rw;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& ordered_value::get<NamedColumn::rw + 1>() {
    return rw;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& ordered_value::get<NamedColumn::rw + 1>() const {
    return rw;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& ordered_value::get<NamedColumn::rw + 2>() {
    return rw;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& ordered_value::get<NamedColumn::rw + 2>() const {
    return rw;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& ordered_value::get<NamedColumn::rw + 3>() {
    return rw;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& ordered_value::get<NamedColumn::rw + 3>() const {
    return rw;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& ordered_value::get<NamedColumn::rw + 4>() {
    return rw;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& ordered_value::get<NamedColumn::rw + 4>() const {
    return rw;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& ordered_value::get<NamedColumn::rw + 5>() {
    return rw;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& ordered_value::get<NamedColumn::rw + 5>() const {
    return rw;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& ordered_value::get<NamedColumn::rw + 6>() {
    return rw;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& ordered_value::get<NamedColumn::rw + 6>() const {
    return rw;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& ordered_value::get<NamedColumn::rw + 7>() {
    return rw;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& ordered_value::get<NamedColumn::rw + 7>() const {
    return rw;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::wo>>& ordered_value::get<NamedColumn::wo + 0>() {
    return wo;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::wo>>& ordered_value::get<NamedColumn::wo + 0>() const {
    return wo;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::wo>>& ordered_value::get<NamedColumn::wo + 1>() {
    return wo;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::wo>>& ordered_value::get<NamedColumn::wo + 1>() const {
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

struct unordered_value;

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::ro> {
    using NamedColumn = unordered_value_datatypes::NamedColumn;
    using struct_type = unordered_value;
    using type = uint64_t;
    using value_type = std::array<uint64_t, 2>;
    static constexpr NamedColumn Column = NamedColumn::ro;
    static constexpr bool is_array = true;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::rw> {
    using NamedColumn = unordered_value_datatypes::NamedColumn;
    using struct_type = unordered_value;
    using type = uint64_t;
    using value_type = std::array<uint64_t, 8>;
    static constexpr NamedColumn Column = NamedColumn::rw;
    static constexpr bool is_array = true;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::wo> {
    using NamedColumn = unordered_value_datatypes::NamedColumn;
    using struct_type = unordered_value;
    using type = uint64_t;
    using value_type = std::array<uint64_t, 2>;
    static constexpr NamedColumn Column = NamedColumn::wo;
    static constexpr bool is_array = true;
    static inline size_t offset();
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
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
            auto split = oldvalue->splitindex_;
            if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Global)) {
                split = ADAPTER_OF(unordered_value)::CurrentSplit();
            }
            if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Inline)) {
                split = split;
            }
            unordered_value::resplit(*this, *oldvalue, split);
        }
    }

    template <NamedColumn Column>
    inline ::sto::adapter::Accessor<accessor_info<RoundedNamedColumn<Column>()>>& get();

    template <NamedColumn Column>
    inline const ::sto::adapter::Accessor<accessor_info<RoundedNamedColumn<Column>()>>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    ::sto::adapter::Accessor<accessor_info<NamedColumn::ro>> ro;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>> rw;
    ::sto::adapter::Accessor<accessor_info<NamedColumn::wo>> wo;
    NamedColumn splitindex_ = DEFAULT_SPLIT;
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

inline size_t accessor_info<NamedColumn::ro>::offset() {
    unordered_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->ro) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::rw>::offset() {
    unordered_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->rw) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::wo>::offset() {
    unordered_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->wo) - reinterpret_cast<uintptr_t>(ptr));
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::ro>>& unordered_value::get<NamedColumn::ro + 0>() {
    return ro;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::ro>>& unordered_value::get<NamedColumn::ro + 0>() const {
    return ro;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::ro>>& unordered_value::get<NamedColumn::ro + 1>() {
    return ro;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::ro>>& unordered_value::get<NamedColumn::ro + 1>() const {
    return ro;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& unordered_value::get<NamedColumn::rw + 0>() {
    return rw;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& unordered_value::get<NamedColumn::rw + 0>() const {
    return rw;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& unordered_value::get<NamedColumn::rw + 1>() {
    return rw;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& unordered_value::get<NamedColumn::rw + 1>() const {
    return rw;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& unordered_value::get<NamedColumn::rw + 2>() {
    return rw;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& unordered_value::get<NamedColumn::rw + 2>() const {
    return rw;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& unordered_value::get<NamedColumn::rw + 3>() {
    return rw;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& unordered_value::get<NamedColumn::rw + 3>() const {
    return rw;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& unordered_value::get<NamedColumn::rw + 4>() {
    return rw;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& unordered_value::get<NamedColumn::rw + 4>() const {
    return rw;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& unordered_value::get<NamedColumn::rw + 5>() {
    return rw;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& unordered_value::get<NamedColumn::rw + 5>() const {
    return rw;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& unordered_value::get<NamedColumn::rw + 6>() {
    return rw;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& unordered_value::get<NamedColumn::rw + 6>() const {
    return rw;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& unordered_value::get<NamedColumn::rw + 7>() {
    return rw;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::rw>>& unordered_value::get<NamedColumn::rw + 7>() const {
    return rw;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::wo>>& unordered_value::get<NamedColumn::wo + 0>() {
    return wo;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::wo>>& unordered_value::get<NamedColumn::wo + 0>() const {
    return wo;
}

template <>
inline ::sto::adapter::Accessor<accessor_info<NamedColumn::wo>>& unordered_value::get<NamedColumn::wo + 1>() {
    return wo;
}

template <>
inline const ::sto::adapter::Accessor<accessor_info<NamedColumn::wo>>& unordered_value::get<NamedColumn::wo + 1>() const {
    return wo;
}

};  // namespace unordered_value_datatypes

using unordered_value = unordered_value_datatypes::unordered_value;

