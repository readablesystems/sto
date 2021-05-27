#pragma once

#include <type_traits>

#include "Adapter.hh"
#include "Sto.hh"

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

template <NamedColumn Column>
struct accessor;

struct warehouse_value;

DEFINE_ADAPTER(warehouse_value, NamedColumn);

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::w_name> {
    using type = var_string<10>;
    using value_type = var_string<10>;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::w_street_1> {
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::w_street_2> {
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::w_city> {
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::w_state> {
    using type = fix_string<2>;
    using value_type = fix_string<2>;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::w_zip> {
    using type = fix_string<9>;
    using value_type = fix_string<9>;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::w_tax> {
    using type = int64_t;
    using value_type = int64_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::w_ytd> {
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr bool is_array = false;
};

template <NamedColumn Column>
struct accessor {
    using adapter_type = ADAPTER_OF(warehouse_value);
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

struct warehouse_value {
    using NamedColumn = warehouse_value_datatypes::NamedColumn;
    static constexpr auto MAX_SPLITS = 2;

    explicit warehouse_value() = default;

    static inline void resplit(
            warehouse_value& newvalue, const warehouse_value& oldvalue, NamedColumn index);

    template <NamedColumn Column>
    static inline void copy_data(warehouse_value& newvalue, const warehouse_value& oldvalue);

    template <NamedColumn Column>
    inline accessor<RoundedNamedColumn<Column>()>& get();

    template <NamedColumn Column>
    inline const accessor<RoundedNamedColumn<Column>()>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    NamedColumn splitindex_ = NamedColumn::COLCOUNT;
    accessor<NamedColumn::w_name> w_name;
    accessor<NamedColumn::w_street_1> w_street_1;
    accessor<NamedColumn::w_street_2> w_street_2;
    accessor<NamedColumn::w_city> w_city;
    accessor<NamedColumn::w_state> w_state;
    accessor<NamedColumn::w_zip> w_zip;
    accessor<NamedColumn::w_tax> w_tax;
    accessor<NamedColumn::w_ytd> w_ytd;
};

inline void warehouse_value::resplit(
        warehouse_value& newvalue, const warehouse_value& oldvalue, NamedColumn index) {
    assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
    memcpy(&newvalue, &oldvalue, sizeof newvalue);
    //copy_data<NamedColumn(0)>(newvalue, oldvalue);
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

template <>
inline accessor<NamedColumn::w_name>& warehouse_value::get<NamedColumn::w_name>() {
    return w_name;
}

template <>
inline const accessor<NamedColumn::w_name>& warehouse_value::get<NamedColumn::w_name>() const {
    return w_name;
}

template <>
inline accessor<NamedColumn::w_street_1>& warehouse_value::get<NamedColumn::w_street_1>() {
    return w_street_1;
}

template <>
inline const accessor<NamedColumn::w_street_1>& warehouse_value::get<NamedColumn::w_street_1>() const {
    return w_street_1;
}

template <>
inline accessor<NamedColumn::w_street_2>& warehouse_value::get<NamedColumn::w_street_2>() {
    return w_street_2;
}

template <>
inline const accessor<NamedColumn::w_street_2>& warehouse_value::get<NamedColumn::w_street_2>() const {
    return w_street_2;
}

template <>
inline accessor<NamedColumn::w_city>& warehouse_value::get<NamedColumn::w_city>() {
    return w_city;
}

template <>
inline const accessor<NamedColumn::w_city>& warehouse_value::get<NamedColumn::w_city>() const {
    return w_city;
}

template <>
inline accessor<NamedColumn::w_state>& warehouse_value::get<NamedColumn::w_state>() {
    return w_state;
}

template <>
inline const accessor<NamedColumn::w_state>& warehouse_value::get<NamedColumn::w_state>() const {
    return w_state;
}

template <>
inline accessor<NamedColumn::w_zip>& warehouse_value::get<NamedColumn::w_zip>() {
    return w_zip;
}

template <>
inline const accessor<NamedColumn::w_zip>& warehouse_value::get<NamedColumn::w_zip>() const {
    return w_zip;
}

template <>
inline accessor<NamedColumn::w_tax>& warehouse_value::get<NamedColumn::w_tax>() {
    return w_tax;
}

template <>
inline const accessor<NamedColumn::w_tax>& warehouse_value::get<NamedColumn::w_tax>() const {
    return w_tax;
}

template <>
inline accessor<NamedColumn::w_ytd>& warehouse_value::get<NamedColumn::w_ytd>() {
    return w_ytd;
}

template <>
inline const accessor<NamedColumn::w_ytd>& warehouse_value::get<NamedColumn::w_ytd>() const {
    return w_ytd;
}

};  // namespace warehouse_value_datatypes

using warehouse_value = warehouse_value_datatypes::warehouse_value;
using ADAPTER_OF(warehouse_value) = ADAPTER_OF(warehouse_value_datatypes::warehouse_value);

#pragma once

#include <type_traits>

#include "Adapter.hh"
#include "Sto.hh"

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

template <NamedColumn Column>
struct accessor;

struct district_value;

DEFINE_ADAPTER(district_value, NamedColumn);

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::d_name> {
    using type = var_string<10>;
    using value_type = var_string<10>;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::d_street_1> {
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::d_street_2> {
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::d_city> {
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::d_state> {
    using type = fix_string<2>;
    using value_type = fix_string<2>;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::d_zip> {
    using type = fix_string<9>;
    using value_type = fix_string<9>;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::d_tax> {
    using type = int64_t;
    using value_type = int64_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::d_ytd> {
    using type = int64_t;
    using value_type = int64_t;
    static constexpr bool is_array = false;
};

template <NamedColumn Column>
struct accessor {
    using adapter_type = ADAPTER_OF(district_value);
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

struct district_value {
    using NamedColumn = district_value_datatypes::NamedColumn;
    static constexpr auto MAX_SPLITS = 2;

    explicit district_value() = default;

    static inline void resplit(
            district_value& newvalue, const district_value& oldvalue, NamedColumn index);

    template <NamedColumn Column>
    static inline void copy_data(district_value& newvalue, const district_value& oldvalue);

    template <NamedColumn Column>
    inline accessor<RoundedNamedColumn<Column>()>& get();

    template <NamedColumn Column>
    inline const accessor<RoundedNamedColumn<Column>()>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    NamedColumn splitindex_ = NamedColumn::COLCOUNT;
    accessor<NamedColumn::d_name> d_name;
    accessor<NamedColumn::d_street_1> d_street_1;
    accessor<NamedColumn::d_street_2> d_street_2;
    accessor<NamedColumn::d_city> d_city;
    accessor<NamedColumn::d_state> d_state;
    accessor<NamedColumn::d_zip> d_zip;
    accessor<NamedColumn::d_tax> d_tax;
    accessor<NamedColumn::d_ytd> d_ytd;
};

inline void district_value::resplit(
        district_value& newvalue, const district_value& oldvalue, NamedColumn index) {
    assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
    memcpy(&newvalue, &oldvalue, sizeof newvalue);
    //copy_data<NamedColumn(0)>(newvalue, oldvalue);
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

template <>
inline accessor<NamedColumn::d_name>& district_value::get<NamedColumn::d_name>() {
    return d_name;
}

template <>
inline const accessor<NamedColumn::d_name>& district_value::get<NamedColumn::d_name>() const {
    return d_name;
}

template <>
inline accessor<NamedColumn::d_street_1>& district_value::get<NamedColumn::d_street_1>() {
    return d_street_1;
}

template <>
inline const accessor<NamedColumn::d_street_1>& district_value::get<NamedColumn::d_street_1>() const {
    return d_street_1;
}

template <>
inline accessor<NamedColumn::d_street_2>& district_value::get<NamedColumn::d_street_2>() {
    return d_street_2;
}

template <>
inline const accessor<NamedColumn::d_street_2>& district_value::get<NamedColumn::d_street_2>() const {
    return d_street_2;
}

template <>
inline accessor<NamedColumn::d_city>& district_value::get<NamedColumn::d_city>() {
    return d_city;
}

template <>
inline const accessor<NamedColumn::d_city>& district_value::get<NamedColumn::d_city>() const {
    return d_city;
}

template <>
inline accessor<NamedColumn::d_state>& district_value::get<NamedColumn::d_state>() {
    return d_state;
}

template <>
inline const accessor<NamedColumn::d_state>& district_value::get<NamedColumn::d_state>() const {
    return d_state;
}

template <>
inline accessor<NamedColumn::d_zip>& district_value::get<NamedColumn::d_zip>() {
    return d_zip;
}

template <>
inline const accessor<NamedColumn::d_zip>& district_value::get<NamedColumn::d_zip>() const {
    return d_zip;
}

template <>
inline accessor<NamedColumn::d_tax>& district_value::get<NamedColumn::d_tax>() {
    return d_tax;
}

template <>
inline const accessor<NamedColumn::d_tax>& district_value::get<NamedColumn::d_tax>() const {
    return d_tax;
}

template <>
inline accessor<NamedColumn::d_ytd>& district_value::get<NamedColumn::d_ytd>() {
    return d_ytd;
}

template <>
inline const accessor<NamedColumn::d_ytd>& district_value::get<NamedColumn::d_ytd>() const {
    return d_ytd;
}

};  // namespace district_value_datatypes

using district_value = district_value_datatypes::district_value;
using ADAPTER_OF(district_value) = ADAPTER_OF(district_value_datatypes::district_value);

#pragma once

#include <type_traits>

#include "Adapter.hh"
#include "Sto.hh"

namespace customer_idx_value_datatypes {

enum class NamedColumn : int {
    c_ids = 0,
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
    return NamedColumn::c_ids;
}

template <NamedColumn Column>
struct accessor;

struct customer_idx_value;

DEFINE_ADAPTER(customer_idx_value, NamedColumn);

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::c_ids> {
    using type = std::list<uint64_t>;
    using value_type = std::list<uint64_t>;
    static constexpr bool is_array = false;
};

template <NamedColumn Column>
struct accessor {
    using adapter_type = ADAPTER_OF(customer_idx_value);
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

struct customer_idx_value {
    using NamedColumn = customer_idx_value_datatypes::NamedColumn;
    static constexpr auto MAX_SPLITS = 2;

    explicit customer_idx_value() = default;

    static inline void resplit(
            customer_idx_value& newvalue, const customer_idx_value& oldvalue, NamedColumn index);

    template <NamedColumn Column>
    static inline void copy_data(customer_idx_value& newvalue, const customer_idx_value& oldvalue);

    template <NamedColumn Column>
    inline accessor<RoundedNamedColumn<Column>()>& get();

    template <NamedColumn Column>
    inline const accessor<RoundedNamedColumn<Column>()>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    NamedColumn splitindex_ = NamedColumn::COLCOUNT;
    accessor<NamedColumn::c_ids> c_ids;
};

inline void customer_idx_value::resplit(
        customer_idx_value& newvalue, const customer_idx_value& oldvalue, NamedColumn index) {
    assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
    memcpy(&newvalue, &oldvalue, sizeof newvalue);
    //copy_data<NamedColumn(0)>(newvalue, oldvalue);
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

template <>
inline accessor<NamedColumn::c_ids>& customer_idx_value::get<NamedColumn::c_ids>() {
    return c_ids;
}

template <>
inline const accessor<NamedColumn::c_ids>& customer_idx_value::get<NamedColumn::c_ids>() const {
    return c_ids;
}

};  // namespace customer_idx_value_datatypes

using customer_idx_value = customer_idx_value_datatypes::customer_idx_value;
using ADAPTER_OF(customer_idx_value) = ADAPTER_OF(customer_idx_value_datatypes::customer_idx_value);

#pragma once

#include <type_traits>

#include "Adapter.hh"
#include "Sto.hh"

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

template <NamedColumn Column>
struct accessor;

struct customer_value;

DEFINE_ADAPTER(customer_value, NamedColumn);

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::c_first> {
    using type = var_string<16>;
    using value_type = var_string<16>;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_middle> {
    using type = fix_string<2>;
    using value_type = fix_string<2>;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_last> {
    using type = var_string<16>;
    using value_type = var_string<16>;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_street_1> {
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_street_2> {
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_city> {
    using type = var_string<20>;
    using value_type = var_string<20>;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_state> {
    using type = fix_string<2>;
    using value_type = fix_string<2>;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_zip> {
    using type = fix_string<9>;
    using value_type = fix_string<9>;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_phone> {
    using type = fix_string<16>;
    using value_type = fix_string<16>;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_since> {
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_credit> {
    using type = fix_string<2>;
    using value_type = fix_string<2>;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_credit_lim> {
    using type = int64_t;
    using value_type = int64_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_discount> {
    using type = int64_t;
    using value_type = int64_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_balance> {
    using type = int64_t;
    using value_type = int64_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_ytd_payment> {
    using type = int64_t;
    using value_type = int64_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_payment_cnt> {
    using type = uint16_t;
    using value_type = uint16_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_delivery_cnt> {
    using type = uint16_t;
    using value_type = uint16_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::c_data> {
    using type = fix_string<500>;
    using value_type = fix_string<500>;
    static constexpr bool is_array = false;
};

template <NamedColumn Column>
struct accessor {
    using adapter_type = ADAPTER_OF(customer_value);
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

struct customer_value {
    using NamedColumn = customer_value_datatypes::NamedColumn;
    static constexpr auto MAX_SPLITS = 2;

    explicit customer_value() = default;

    static inline void resplit(
            customer_value& newvalue, const customer_value& oldvalue, NamedColumn index);

    template <NamedColumn Column>
    static inline void copy_data(customer_value& newvalue, const customer_value& oldvalue);

    template <NamedColumn Column>
    inline accessor<RoundedNamedColumn<Column>()>& get();

    template <NamedColumn Column>
    inline const accessor<RoundedNamedColumn<Column>()>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    NamedColumn splitindex_ = NamedColumn::COLCOUNT;
    accessor<NamedColumn::c_first> c_first;
    accessor<NamedColumn::c_middle> c_middle;
    accessor<NamedColumn::c_last> c_last;
    accessor<NamedColumn::c_street_1> c_street_1;
    accessor<NamedColumn::c_street_2> c_street_2;
    accessor<NamedColumn::c_city> c_city;
    accessor<NamedColumn::c_state> c_state;
    accessor<NamedColumn::c_zip> c_zip;
    accessor<NamedColumn::c_phone> c_phone;
    accessor<NamedColumn::c_since> c_since;
    accessor<NamedColumn::c_credit> c_credit;
    accessor<NamedColumn::c_credit_lim> c_credit_lim;
    accessor<NamedColumn::c_discount> c_discount;
    accessor<NamedColumn::c_balance> c_balance;
    accessor<NamedColumn::c_ytd_payment> c_ytd_payment;
    accessor<NamedColumn::c_payment_cnt> c_payment_cnt;
    accessor<NamedColumn::c_delivery_cnt> c_delivery_cnt;
    accessor<NamedColumn::c_data> c_data;
};

inline void customer_value::resplit(
        customer_value& newvalue, const customer_value& oldvalue, NamedColumn index) {
    assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
    memcpy(&newvalue, &oldvalue, sizeof newvalue);
    //copy_data<NamedColumn(0)>(newvalue, oldvalue);
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

template <>
inline accessor<NamedColumn::c_first>& customer_value::get<NamedColumn::c_first>() {
    return c_first;
}

template <>
inline const accessor<NamedColumn::c_first>& customer_value::get<NamedColumn::c_first>() const {
    return c_first;
}

template <>
inline accessor<NamedColumn::c_middle>& customer_value::get<NamedColumn::c_middle>() {
    return c_middle;
}

template <>
inline const accessor<NamedColumn::c_middle>& customer_value::get<NamedColumn::c_middle>() const {
    return c_middle;
}

template <>
inline accessor<NamedColumn::c_last>& customer_value::get<NamedColumn::c_last>() {
    return c_last;
}

template <>
inline const accessor<NamedColumn::c_last>& customer_value::get<NamedColumn::c_last>() const {
    return c_last;
}

template <>
inline accessor<NamedColumn::c_street_1>& customer_value::get<NamedColumn::c_street_1>() {
    return c_street_1;
}

template <>
inline const accessor<NamedColumn::c_street_1>& customer_value::get<NamedColumn::c_street_1>() const {
    return c_street_1;
}

template <>
inline accessor<NamedColumn::c_street_2>& customer_value::get<NamedColumn::c_street_2>() {
    return c_street_2;
}

template <>
inline const accessor<NamedColumn::c_street_2>& customer_value::get<NamedColumn::c_street_2>() const {
    return c_street_2;
}

template <>
inline accessor<NamedColumn::c_city>& customer_value::get<NamedColumn::c_city>() {
    return c_city;
}

template <>
inline const accessor<NamedColumn::c_city>& customer_value::get<NamedColumn::c_city>() const {
    return c_city;
}

template <>
inline accessor<NamedColumn::c_state>& customer_value::get<NamedColumn::c_state>() {
    return c_state;
}

template <>
inline const accessor<NamedColumn::c_state>& customer_value::get<NamedColumn::c_state>() const {
    return c_state;
}

template <>
inline accessor<NamedColumn::c_zip>& customer_value::get<NamedColumn::c_zip>() {
    return c_zip;
}

template <>
inline const accessor<NamedColumn::c_zip>& customer_value::get<NamedColumn::c_zip>() const {
    return c_zip;
}

template <>
inline accessor<NamedColumn::c_phone>& customer_value::get<NamedColumn::c_phone>() {
    return c_phone;
}

template <>
inline const accessor<NamedColumn::c_phone>& customer_value::get<NamedColumn::c_phone>() const {
    return c_phone;
}

template <>
inline accessor<NamedColumn::c_since>& customer_value::get<NamedColumn::c_since>() {
    return c_since;
}

template <>
inline const accessor<NamedColumn::c_since>& customer_value::get<NamedColumn::c_since>() const {
    return c_since;
}

template <>
inline accessor<NamedColumn::c_credit>& customer_value::get<NamedColumn::c_credit>() {
    return c_credit;
}

template <>
inline const accessor<NamedColumn::c_credit>& customer_value::get<NamedColumn::c_credit>() const {
    return c_credit;
}

template <>
inline accessor<NamedColumn::c_credit_lim>& customer_value::get<NamedColumn::c_credit_lim>() {
    return c_credit_lim;
}

template <>
inline const accessor<NamedColumn::c_credit_lim>& customer_value::get<NamedColumn::c_credit_lim>() const {
    return c_credit_lim;
}

template <>
inline accessor<NamedColumn::c_discount>& customer_value::get<NamedColumn::c_discount>() {
    return c_discount;
}

template <>
inline const accessor<NamedColumn::c_discount>& customer_value::get<NamedColumn::c_discount>() const {
    return c_discount;
}

template <>
inline accessor<NamedColumn::c_balance>& customer_value::get<NamedColumn::c_balance>() {
    return c_balance;
}

template <>
inline const accessor<NamedColumn::c_balance>& customer_value::get<NamedColumn::c_balance>() const {
    return c_balance;
}

template <>
inline accessor<NamedColumn::c_ytd_payment>& customer_value::get<NamedColumn::c_ytd_payment>() {
    return c_ytd_payment;
}

template <>
inline const accessor<NamedColumn::c_ytd_payment>& customer_value::get<NamedColumn::c_ytd_payment>() const {
    return c_ytd_payment;
}

template <>
inline accessor<NamedColumn::c_payment_cnt>& customer_value::get<NamedColumn::c_payment_cnt>() {
    return c_payment_cnt;
}

template <>
inline const accessor<NamedColumn::c_payment_cnt>& customer_value::get<NamedColumn::c_payment_cnt>() const {
    return c_payment_cnt;
}

template <>
inline accessor<NamedColumn::c_delivery_cnt>& customer_value::get<NamedColumn::c_delivery_cnt>() {
    return c_delivery_cnt;
}

template <>
inline const accessor<NamedColumn::c_delivery_cnt>& customer_value::get<NamedColumn::c_delivery_cnt>() const {
    return c_delivery_cnt;
}

template <>
inline accessor<NamedColumn::c_data>& customer_value::get<NamedColumn::c_data>() {
    return c_data;
}

template <>
inline const accessor<NamedColumn::c_data>& customer_value::get<NamedColumn::c_data>() const {
    return c_data;
}

};  // namespace customer_value_datatypes

using customer_value = customer_value_datatypes::customer_value;
using ADAPTER_OF(customer_value) = ADAPTER_OF(customer_value_datatypes::customer_value);

#pragma once

#include <type_traits>

#include "Adapter.hh"
#include "Sto.hh"

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

template <NamedColumn Column>
struct accessor;

struct history_value;

DEFINE_ADAPTER(history_value, NamedColumn);

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::h_c_id> {
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::h_c_d_id> {
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::h_c_w_id> {
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::h_d_id> {
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::h_w_id> {
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::h_date> {
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::h_amount> {
    using type = int64_t;
    using value_type = int64_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::h_data> {
    using type = var_string<24>;
    using value_type = var_string<24>;
    static constexpr bool is_array = false;
};

template <NamedColumn Column>
struct accessor {
    using adapter_type = ADAPTER_OF(history_value);
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

struct history_value {
    using NamedColumn = history_value_datatypes::NamedColumn;
    static constexpr auto MAX_SPLITS = 2;

    explicit history_value() = default;

    static inline void resplit(
            history_value& newvalue, const history_value& oldvalue, NamedColumn index);

    template <NamedColumn Column>
    static inline void copy_data(history_value& newvalue, const history_value& oldvalue);

    template <NamedColumn Column>
    inline accessor<RoundedNamedColumn<Column>()>& get();

    template <NamedColumn Column>
    inline const accessor<RoundedNamedColumn<Column>()>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    NamedColumn splitindex_ = NamedColumn::COLCOUNT;
    accessor<NamedColumn::h_c_id> h_c_id;
    accessor<NamedColumn::h_c_d_id> h_c_d_id;
    accessor<NamedColumn::h_c_w_id> h_c_w_id;
    accessor<NamedColumn::h_d_id> h_d_id;
    accessor<NamedColumn::h_w_id> h_w_id;
    accessor<NamedColumn::h_date> h_date;
    accessor<NamedColumn::h_amount> h_amount;
    accessor<NamedColumn::h_data> h_data;
};

inline void history_value::resplit(
        history_value& newvalue, const history_value& oldvalue, NamedColumn index) {
    assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
    memcpy(&newvalue, &oldvalue, sizeof newvalue);
    //copy_data<NamedColumn(0)>(newvalue, oldvalue);
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

template <>
inline accessor<NamedColumn::h_c_id>& history_value::get<NamedColumn::h_c_id>() {
    return h_c_id;
}

template <>
inline const accessor<NamedColumn::h_c_id>& history_value::get<NamedColumn::h_c_id>() const {
    return h_c_id;
}

template <>
inline accessor<NamedColumn::h_c_d_id>& history_value::get<NamedColumn::h_c_d_id>() {
    return h_c_d_id;
}

template <>
inline const accessor<NamedColumn::h_c_d_id>& history_value::get<NamedColumn::h_c_d_id>() const {
    return h_c_d_id;
}

template <>
inline accessor<NamedColumn::h_c_w_id>& history_value::get<NamedColumn::h_c_w_id>() {
    return h_c_w_id;
}

template <>
inline const accessor<NamedColumn::h_c_w_id>& history_value::get<NamedColumn::h_c_w_id>() const {
    return h_c_w_id;
}

template <>
inline accessor<NamedColumn::h_d_id>& history_value::get<NamedColumn::h_d_id>() {
    return h_d_id;
}

template <>
inline const accessor<NamedColumn::h_d_id>& history_value::get<NamedColumn::h_d_id>() const {
    return h_d_id;
}

template <>
inline accessor<NamedColumn::h_w_id>& history_value::get<NamedColumn::h_w_id>() {
    return h_w_id;
}

template <>
inline const accessor<NamedColumn::h_w_id>& history_value::get<NamedColumn::h_w_id>() const {
    return h_w_id;
}

template <>
inline accessor<NamedColumn::h_date>& history_value::get<NamedColumn::h_date>() {
    return h_date;
}

template <>
inline const accessor<NamedColumn::h_date>& history_value::get<NamedColumn::h_date>() const {
    return h_date;
}

template <>
inline accessor<NamedColumn::h_amount>& history_value::get<NamedColumn::h_amount>() {
    return h_amount;
}

template <>
inline const accessor<NamedColumn::h_amount>& history_value::get<NamedColumn::h_amount>() const {
    return h_amount;
}

template <>
inline accessor<NamedColumn::h_data>& history_value::get<NamedColumn::h_data>() {
    return h_data;
}

template <>
inline const accessor<NamedColumn::h_data>& history_value::get<NamedColumn::h_data>() const {
    return h_data;
}

};  // namespace history_value_datatypes

using history_value = history_value_datatypes::history_value;
using ADAPTER_OF(history_value) = ADAPTER_OF(history_value_datatypes::history_value);

#pragma once

#include <type_traits>

#include "Adapter.hh"
#include "Sto.hh"

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

template <NamedColumn Column>
struct accessor;

struct order_value;

DEFINE_ADAPTER(order_value, NamedColumn);

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::o_c_id> {
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::o_entry_d> {
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::o_ol_cnt> {
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::o_all_local> {
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::o_carrier_id> {
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr bool is_array = false;
};

template <NamedColumn Column>
struct accessor {
    using adapter_type = ADAPTER_OF(order_value);
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

struct order_value {
    using NamedColumn = order_value_datatypes::NamedColumn;
    static constexpr auto MAX_SPLITS = 2;

    explicit order_value() = default;

    static inline void resplit(
            order_value& newvalue, const order_value& oldvalue, NamedColumn index);

    template <NamedColumn Column>
    static inline void copy_data(order_value& newvalue, const order_value& oldvalue);

    template <NamedColumn Column>
    inline accessor<RoundedNamedColumn<Column>()>& get();

    template <NamedColumn Column>
    inline const accessor<RoundedNamedColumn<Column>()>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    NamedColumn splitindex_ = NamedColumn::COLCOUNT;
    accessor<NamedColumn::o_c_id> o_c_id;
    accessor<NamedColumn::o_entry_d> o_entry_d;
    accessor<NamedColumn::o_ol_cnt> o_ol_cnt;
    accessor<NamedColumn::o_all_local> o_all_local;
    accessor<NamedColumn::o_carrier_id> o_carrier_id;
};

inline void order_value::resplit(
        order_value& newvalue, const order_value& oldvalue, NamedColumn index) {
    assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
    memcpy(&newvalue, &oldvalue, sizeof newvalue);
    //copy_data<NamedColumn(0)>(newvalue, oldvalue);
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

template <>
inline accessor<NamedColumn::o_c_id>& order_value::get<NamedColumn::o_c_id>() {
    return o_c_id;
}

template <>
inline const accessor<NamedColumn::o_c_id>& order_value::get<NamedColumn::o_c_id>() const {
    return o_c_id;
}

template <>
inline accessor<NamedColumn::o_entry_d>& order_value::get<NamedColumn::o_entry_d>() {
    return o_entry_d;
}

template <>
inline const accessor<NamedColumn::o_entry_d>& order_value::get<NamedColumn::o_entry_d>() const {
    return o_entry_d;
}

template <>
inline accessor<NamedColumn::o_ol_cnt>& order_value::get<NamedColumn::o_ol_cnt>() {
    return o_ol_cnt;
}

template <>
inline const accessor<NamedColumn::o_ol_cnt>& order_value::get<NamedColumn::o_ol_cnt>() const {
    return o_ol_cnt;
}

template <>
inline accessor<NamedColumn::o_all_local>& order_value::get<NamedColumn::o_all_local>() {
    return o_all_local;
}

template <>
inline const accessor<NamedColumn::o_all_local>& order_value::get<NamedColumn::o_all_local>() const {
    return o_all_local;
}

template <>
inline accessor<NamedColumn::o_carrier_id>& order_value::get<NamedColumn::o_carrier_id>() {
    return o_carrier_id;
}

template <>
inline const accessor<NamedColumn::o_carrier_id>& order_value::get<NamedColumn::o_carrier_id>() const {
    return o_carrier_id;
}

};  // namespace order_value_datatypes

using order_value = order_value_datatypes::order_value;
using ADAPTER_OF(order_value) = ADAPTER_OF(order_value_datatypes::order_value);

#pragma once

#include <type_traits>

#include "Adapter.hh"
#include "Sto.hh"

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

template <NamedColumn Column>
struct accessor;

struct orderline_value;

DEFINE_ADAPTER(orderline_value, NamedColumn);

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::ol_i_id> {
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::ol_supply_w_id> {
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::ol_quantity> {
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::ol_amount> {
    using type = int32_t;
    using value_type = int32_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::ol_dist_info> {
    using type = fix_string<24>;
    using value_type = fix_string<24>;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::ol_delivery_d> {
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr bool is_array = false;
};

template <NamedColumn Column>
struct accessor {
    using adapter_type = ADAPTER_OF(orderline_value);
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

struct orderline_value {
    using NamedColumn = orderline_value_datatypes::NamedColumn;
    static constexpr auto MAX_SPLITS = 2;

    explicit orderline_value() = default;

    static inline void resplit(
            orderline_value& newvalue, const orderline_value& oldvalue, NamedColumn index);

    template <NamedColumn Column>
    static inline void copy_data(orderline_value& newvalue, const orderline_value& oldvalue);

    template <NamedColumn Column>
    inline accessor<RoundedNamedColumn<Column>()>& get();

    template <NamedColumn Column>
    inline const accessor<RoundedNamedColumn<Column>()>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    NamedColumn splitindex_ = NamedColumn::COLCOUNT;
    accessor<NamedColumn::ol_i_id> ol_i_id;
    accessor<NamedColumn::ol_supply_w_id> ol_supply_w_id;
    accessor<NamedColumn::ol_quantity> ol_quantity;
    accessor<NamedColumn::ol_amount> ol_amount;
    accessor<NamedColumn::ol_dist_info> ol_dist_info;
    accessor<NamedColumn::ol_delivery_d> ol_delivery_d;
};

inline void orderline_value::resplit(
        orderline_value& newvalue, const orderline_value& oldvalue, NamedColumn index) {
    assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
    memcpy(&newvalue, &oldvalue, sizeof newvalue);
    //copy_data<NamedColumn(0)>(newvalue, oldvalue);
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

template <>
inline accessor<NamedColumn::ol_i_id>& orderline_value::get<NamedColumn::ol_i_id>() {
    return ol_i_id;
}

template <>
inline const accessor<NamedColumn::ol_i_id>& orderline_value::get<NamedColumn::ol_i_id>() const {
    return ol_i_id;
}

template <>
inline accessor<NamedColumn::ol_supply_w_id>& orderline_value::get<NamedColumn::ol_supply_w_id>() {
    return ol_supply_w_id;
}

template <>
inline const accessor<NamedColumn::ol_supply_w_id>& orderline_value::get<NamedColumn::ol_supply_w_id>() const {
    return ol_supply_w_id;
}

template <>
inline accessor<NamedColumn::ol_quantity>& orderline_value::get<NamedColumn::ol_quantity>() {
    return ol_quantity;
}

template <>
inline const accessor<NamedColumn::ol_quantity>& orderline_value::get<NamedColumn::ol_quantity>() const {
    return ol_quantity;
}

template <>
inline accessor<NamedColumn::ol_amount>& orderline_value::get<NamedColumn::ol_amount>() {
    return ol_amount;
}

template <>
inline const accessor<NamedColumn::ol_amount>& orderline_value::get<NamedColumn::ol_amount>() const {
    return ol_amount;
}

template <>
inline accessor<NamedColumn::ol_dist_info>& orderline_value::get<NamedColumn::ol_dist_info>() {
    return ol_dist_info;
}

template <>
inline const accessor<NamedColumn::ol_dist_info>& orderline_value::get<NamedColumn::ol_dist_info>() const {
    return ol_dist_info;
}

template <>
inline accessor<NamedColumn::ol_delivery_d>& orderline_value::get<NamedColumn::ol_delivery_d>() {
    return ol_delivery_d;
}

template <>
inline const accessor<NamedColumn::ol_delivery_d>& orderline_value::get<NamedColumn::ol_delivery_d>() const {
    return ol_delivery_d;
}

};  // namespace orderline_value_datatypes

using orderline_value = orderline_value_datatypes::orderline_value;
using ADAPTER_OF(orderline_value) = ADAPTER_OF(orderline_value_datatypes::orderline_value);

#pragma once

#include <type_traits>

#include "Adapter.hh"
#include "Sto.hh"

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

template <NamedColumn Column>
struct accessor;

struct item_value;

DEFINE_ADAPTER(item_value, NamedColumn);

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::i_im_id> {
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::i_price> {
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::i_name> {
    using type = var_string<24>;
    using value_type = var_string<24>;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::i_data> {
    using type = var_string<50>;
    using value_type = var_string<50>;
    static constexpr bool is_array = false;
};

template <NamedColumn Column>
struct accessor {
    using adapter_type = ADAPTER_OF(item_value);
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

struct item_value {
    using NamedColumn = item_value_datatypes::NamedColumn;
    static constexpr auto MAX_SPLITS = 2;

    explicit item_value() = default;

    static inline void resplit(
            item_value& newvalue, const item_value& oldvalue, NamedColumn index);

    template <NamedColumn Column>
    static inline void copy_data(item_value& newvalue, const item_value& oldvalue);

    template <NamedColumn Column>
    inline accessor<RoundedNamedColumn<Column>()>& get();

    template <NamedColumn Column>
    inline const accessor<RoundedNamedColumn<Column>()>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    NamedColumn splitindex_ = NamedColumn::COLCOUNT;
    accessor<NamedColumn::i_im_id> i_im_id;
    accessor<NamedColumn::i_price> i_price;
    accessor<NamedColumn::i_name> i_name;
    accessor<NamedColumn::i_data> i_data;
};

inline void item_value::resplit(
        item_value& newvalue, const item_value& oldvalue, NamedColumn index) {
    assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
    memcpy(&newvalue, &oldvalue, sizeof newvalue);
    //copy_data<NamedColumn(0)>(newvalue, oldvalue);
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

template <>
inline accessor<NamedColumn::i_im_id>& item_value::get<NamedColumn::i_im_id>() {
    return i_im_id;
}

template <>
inline const accessor<NamedColumn::i_im_id>& item_value::get<NamedColumn::i_im_id>() const {
    return i_im_id;
}

template <>
inline accessor<NamedColumn::i_price>& item_value::get<NamedColumn::i_price>() {
    return i_price;
}

template <>
inline const accessor<NamedColumn::i_price>& item_value::get<NamedColumn::i_price>() const {
    return i_price;
}

template <>
inline accessor<NamedColumn::i_name>& item_value::get<NamedColumn::i_name>() {
    return i_name;
}

template <>
inline const accessor<NamedColumn::i_name>& item_value::get<NamedColumn::i_name>() const {
    return i_name;
}

template <>
inline accessor<NamedColumn::i_data>& item_value::get<NamedColumn::i_data>() {
    return i_data;
}

template <>
inline const accessor<NamedColumn::i_data>& item_value::get<NamedColumn::i_data>() const {
    return i_data;
}

};  // namespace item_value_datatypes

using item_value = item_value_datatypes::item_value;
using ADAPTER_OF(item_value) = ADAPTER_OF(item_value_datatypes::item_value);

#pragma once

#include <type_traits>

#include "Adapter.hh"
#include "Sto.hh"

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

template <NamedColumn Column>
struct accessor;

struct stock_value;

DEFINE_ADAPTER(stock_value, NamedColumn);

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::s_dists> {
    using type = fix_string<24>;
    using value_type = std::array<fix_string<24>, 10>;
    static constexpr bool is_array = true;
};

template <>
struct accessor_info<NamedColumn::s_data> {
    using type = var_string<50>;
    using value_type = var_string<50>;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::s_quantity> {
    using type = int32_t;
    using value_type = int32_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::s_ytd> {
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::s_order_cnt> {
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::s_remote_cnt> {
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr bool is_array = false;
};

template <NamedColumn Column>
struct accessor {
    using adapter_type = ADAPTER_OF(stock_value);
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

struct stock_value {
    using NamedColumn = stock_value_datatypes::NamedColumn;
    static constexpr auto MAX_SPLITS = 2;

    explicit stock_value() = default;

    static inline void resplit(
            stock_value& newvalue, const stock_value& oldvalue, NamedColumn index);

    template <NamedColumn Column>
    static inline void copy_data(stock_value& newvalue, const stock_value& oldvalue);

    template <NamedColumn Column>
    inline accessor<RoundedNamedColumn<Column>()>& get();

    template <NamedColumn Column>
    inline const accessor<RoundedNamedColumn<Column>()>& get() const;


    const auto split_of(NamedColumn index) const {
        return index < splitindex_ ? 0 : 1;
    }

    NamedColumn splitindex_ = NamedColumn::COLCOUNT;
    accessor<NamedColumn::s_dists> s_dists;
    accessor<NamedColumn::s_data> s_data;
    accessor<NamedColumn::s_quantity> s_quantity;
    accessor<NamedColumn::s_ytd> s_ytd;
    accessor<NamedColumn::s_order_cnt> s_order_cnt;
    accessor<NamedColumn::s_remote_cnt> s_remote_cnt;
};

inline void stock_value::resplit(
        stock_value& newvalue, const stock_value& oldvalue, NamedColumn index) {
    assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
    memcpy(&newvalue, &oldvalue, sizeof newvalue);
    //copy_data<NamedColumn(0)>(newvalue, oldvalue);
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

template <>
inline accessor<NamedColumn::s_dists>& stock_value::get<NamedColumn::s_dists + 0>() {
    return s_dists;
}

template <>
inline const accessor<NamedColumn::s_dists>& stock_value::get<NamedColumn::s_dists + 0>() const {
    return s_dists;
}

template <>
inline accessor<NamedColumn::s_dists>& stock_value::get<NamedColumn::s_dists + 1>() {
    return s_dists;
}

template <>
inline const accessor<NamedColumn::s_dists>& stock_value::get<NamedColumn::s_dists + 1>() const {
    return s_dists;
}

template <>
inline accessor<NamedColumn::s_dists>& stock_value::get<NamedColumn::s_dists + 2>() {
    return s_dists;
}

template <>
inline const accessor<NamedColumn::s_dists>& stock_value::get<NamedColumn::s_dists + 2>() const {
    return s_dists;
}

template <>
inline accessor<NamedColumn::s_dists>& stock_value::get<NamedColumn::s_dists + 3>() {
    return s_dists;
}

template <>
inline const accessor<NamedColumn::s_dists>& stock_value::get<NamedColumn::s_dists + 3>() const {
    return s_dists;
}

template <>
inline accessor<NamedColumn::s_dists>& stock_value::get<NamedColumn::s_dists + 4>() {
    return s_dists;
}

template <>
inline const accessor<NamedColumn::s_dists>& stock_value::get<NamedColumn::s_dists + 4>() const {
    return s_dists;
}

template <>
inline accessor<NamedColumn::s_dists>& stock_value::get<NamedColumn::s_dists + 5>() {
    return s_dists;
}

template <>
inline const accessor<NamedColumn::s_dists>& stock_value::get<NamedColumn::s_dists + 5>() const {
    return s_dists;
}

template <>
inline accessor<NamedColumn::s_dists>& stock_value::get<NamedColumn::s_dists + 6>() {
    return s_dists;
}

template <>
inline const accessor<NamedColumn::s_dists>& stock_value::get<NamedColumn::s_dists + 6>() const {
    return s_dists;
}

template <>
inline accessor<NamedColumn::s_dists>& stock_value::get<NamedColumn::s_dists + 7>() {
    return s_dists;
}

template <>
inline const accessor<NamedColumn::s_dists>& stock_value::get<NamedColumn::s_dists + 7>() const {
    return s_dists;
}

template <>
inline accessor<NamedColumn::s_dists>& stock_value::get<NamedColumn::s_dists + 8>() {
    return s_dists;
}

template <>
inline const accessor<NamedColumn::s_dists>& stock_value::get<NamedColumn::s_dists + 8>() const {
    return s_dists;
}

template <>
inline accessor<NamedColumn::s_dists>& stock_value::get<NamedColumn::s_dists + 9>() {
    return s_dists;
}

template <>
inline const accessor<NamedColumn::s_dists>& stock_value::get<NamedColumn::s_dists + 9>() const {
    return s_dists;
}

template <>
inline accessor<NamedColumn::s_data>& stock_value::get<NamedColumn::s_data>() {
    return s_data;
}

template <>
inline const accessor<NamedColumn::s_data>& stock_value::get<NamedColumn::s_data>() const {
    return s_data;
}

template <>
inline accessor<NamedColumn::s_quantity>& stock_value::get<NamedColumn::s_quantity>() {
    return s_quantity;
}

template <>
inline const accessor<NamedColumn::s_quantity>& stock_value::get<NamedColumn::s_quantity>() const {
    return s_quantity;
}

template <>
inline accessor<NamedColumn::s_ytd>& stock_value::get<NamedColumn::s_ytd>() {
    return s_ytd;
}

template <>
inline const accessor<NamedColumn::s_ytd>& stock_value::get<NamedColumn::s_ytd>() const {
    return s_ytd;
}

template <>
inline accessor<NamedColumn::s_order_cnt>& stock_value::get<NamedColumn::s_order_cnt>() {
    return s_order_cnt;
}

template <>
inline const accessor<NamedColumn::s_order_cnt>& stock_value::get<NamedColumn::s_order_cnt>() const {
    return s_order_cnt;
}

template <>
inline accessor<NamedColumn::s_remote_cnt>& stock_value::get<NamedColumn::s_remote_cnt>() {
    return s_remote_cnt;
}

template <>
inline const accessor<NamedColumn::s_remote_cnt>& stock_value::get<NamedColumn::s_remote_cnt>() const {
    return s_remote_cnt;
}

};  // namespace stock_value_datatypes

using stock_value = stock_value_datatypes::stock_value;
using ADAPTER_OF(stock_value) = ADAPTER_OF(stock_value_datatypes::stock_value);

