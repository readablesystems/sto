#pragma once

#include <type_traits>

namespace ycsb {

namespace ycsb_value_datatypes {

enum class NamedColumn : int {
    even_columns = 0,
    odd_columns = 5,
    COLCOUNT = 10
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
    if constexpr (Column < NamedColumn::odd_columns) {
        return NamedColumn::even_columns;
    }
    return NamedColumn::odd_columns;
}

template <NamedColumn Column>
struct accessor;

struct ycsb_value;

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::even_columns> {
    using type = ::bench::fix_string<COL_WIDTH>;
    using value_type = std::array<::bench::fix_string<COL_WIDTH>, 5>;
    static constexpr bool is_array = true;
    static inline size_t offset();
};

template <>
struct accessor_info<NamedColumn::odd_columns> {
    using type = ::bench::fix_string<COL_WIDTH>;
    using value_type = std::array<::bench::fix_string<COL_WIDTH>, 5>;
    static constexpr bool is_array = true;
    static inline size_t offset();
};

template <NamedColumn Column>
struct accessor {
    using adapter_type = ::sto::GlobalAdapter<ycsb_value, NamedColumn>;
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
    static constexpr auto DEFAULT_SPLIT = NamedColumn::COLCOUNT;
    static constexpr auto MAX_SPLITS = 2;

    explicit ycsb_value() = default;

    static inline void resplit(
            ycsb_value& newvalue, const ycsb_value& oldvalue, NamedColumn index);

    template <NamedColumn Column>
    static inline void copy_data(ycsb_value& newvalue, const ycsb_value& oldvalue);

    inline void init(const ycsb_value* oldvalue = nullptr) {
        if (oldvalue) {
            auto split = oldvalue->splitindex_;
            if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Global)) {
                split = ADAPTER_OF(ycsb_value)::CurrentSplit();
            }
            if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Inline)) {
                split = split;
            }
            ycsb_value::resplit(*this, *oldvalue, split);
        }
    }

    template <NamedColumn Column>
    inline accessor<RoundedNamedColumn<Column>()>& get();

    template <NamedColumn Column>
    inline const accessor<RoundedNamedColumn<Column>()>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    accessor<NamedColumn::even_columns> even_columns;
    accessor<NamedColumn::odd_columns> odd_columns;
    NamedColumn splitindex_ = DEFAULT_SPLIT;
};

inline void ycsb_value::resplit(
        ycsb_value& newvalue, const ycsb_value& oldvalue, NamedColumn index) {
    assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
    if (&newvalue != &oldvalue) {
        memcpy(&newvalue, &oldvalue, sizeof newvalue);
        //copy_data<NamedColumn(0)>(newvalue, oldvalue);
    }
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

inline size_t accessor_info<NamedColumn::even_columns>::offset() {
    ycsb_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->even_columns) - reinterpret_cast<uintptr_t>(ptr));
}

inline size_t accessor_info<NamedColumn::odd_columns>::offset() {
    ycsb_value* ptr = nullptr;
    return static_cast<size_t>(
        reinterpret_cast<uintptr_t>(&ptr->odd_columns) - reinterpret_cast<uintptr_t>(ptr));
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

};  // namespace ycsb

