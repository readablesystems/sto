#pragma once

#include "Adapter.hh"
#include "Sto.hh"
#include "VersionSelector.hh"

namespace index_value_datatypes {

enum class NamedColumn : int {
    value_1 = 0,
    value_2a,
    value_2b,
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

std::ostream& operator<<(std::ostream& out, NamedColumn& nc) {
    out << static_cast<std::underlying_type_t<NamedColumn>>(nc);
    return out;
}

template <NamedColumn Column>
constexpr NamedColumn RoundedNamedColumn() {
    static_assert(Column < NamedColumn::COLCOUNT);
    if constexpr (Column < NamedColumn::value_2a) {
        return NamedColumn::value_1;
    }
    if constexpr (Column < NamedColumn::value_2b) {
        return NamedColumn::value_2a;
    }
    return NamedColumn::value_2b;
}

template <NamedColumn Column>
struct accessor;

template <NamedColumn StartIndex, NamedColumn EndIndex>
struct split_value;

template <NamedColumn SplitIndex>
struct unified_value;

struct index_value;

DEFINE_ADAPTER(index_value, NamedColumn);

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::value_1> {
    using type = int64_t;
};

template <>
struct accessor_info<NamedColumn::value_2a> {
    using type = int64_t;
};

template <>
struct accessor_info<NamedColumn::value_2b> {
    using type = int64_t;
};

template <NamedColumn Column>
struct accessor {
    using type = typename accessor_info<Column>::type;

    accessor() = default;
    accessor(type& value) : value_(value) {}
    accessor(const type& value) : value_(const_cast<type&>(value)) {}

    operator type() {
        ADAPTER_OF(index_value)::CountWrite(Column + index_);
        return value_;
    }

    operator const type() const {
        ADAPTER_OF(index_value)::CountRead(Column + index_);
        return value_;
    }

    operator type&() {
        ADAPTER_OF(index_value)::CountWrite(Column + index_);
        return value_;
    }

    operator const type&() const {
        ADAPTER_OF(index_value)::CountRead(Column + index_);
        return value_;
    }

    type operator =(const type& other) {
        ADAPTER_OF(index_value)::CountWrite(Column + index_);
        return value_ = other;
    }

    type operator =(const accessor<Column>& other) {
        ADAPTER_OF(index_value)::CountWrite(Column + index_);
        return value_ = other.value_;
    }

    type operator *() {
        ADAPTER_OF(index_value)::CountWrite(Column + index_);
        return value_;
    }

    const type operator *() const {
        ADAPTER_OF(index_value)::CountRead(Column + index_);
        return value_;
    }

    type* operator ->() {
        ADAPTER_OF(index_value)::CountWrite(Column + index_);
        return &value_;
    }

    const type* operator ->() const {
        ADAPTER_OF(index_value)::CountRead(Column + index_);
        return &value_;
    }

    std::underlying_type_t<NamedColumn> index_ = 0;
    type value_;
};

template <>
struct split_value<NamedColumn(0), NamedColumn(1)> {
    explicit split_value() = default;
    accessor<NamedColumn::value_1> value_1;
};
template <>
struct split_value<NamedColumn(1), NamedColumn(3)> {
    explicit split_value() = default;
    accessor<NamedColumn::value_2a> value_2a;
    accessor<NamedColumn::value_2b> value_2b;
};
template <>
struct unified_value<NamedColumn(1)> {
    auto& value_1() {
        return split_0.value_1;
    }
    const auto& value_1() const {
        return split_0.value_1;
    }
    auto& value_2a() {
        return split_1.value_2a;
    }
    const auto& value_2a() const {
        return split_1.value_2a;
    }
    auto& value_2b() {
        return split_1.value_2b;
    }
    const auto& value_2b() const {
        return split_1.value_2b;
    }

    const auto split_of(NamedColumn index) const {
        return index < NamedColumn(1) ? 0 : 1;
    }

    split_value<NamedColumn(0), NamedColumn(1)> split_0;
    split_value<NamedColumn(1), NamedColumn(3)> split_1;
};

template <>
struct split_value<NamedColumn(0), NamedColumn(2)> {
    explicit split_value() = default;
    accessor<NamedColumn::value_1> value_1;
    accessor<NamedColumn::value_2a> value_2a;
};
template <>
struct split_value<NamedColumn(2), NamedColumn(3)> {
    explicit split_value() = default;
    accessor<NamedColumn::value_2b> value_2b;
};
template <>
struct unified_value<NamedColumn(2)> {
    auto& value_1() {
        return split_0.value_1;
    }
    const auto& value_1() const {
        return split_0.value_1;
    }
    auto& value_2a() {
        return split_0.value_2a;
    }
    const auto& value_2a() const {
        return split_0.value_2a;
    }
    auto& value_2b() {
        return split_1.value_2b;
    }
    const auto& value_2b() const {
        return split_1.value_2b;
    }

    const auto split_of(NamedColumn index) const {
        return index < NamedColumn(2) ? 0 : 1;
    }

    split_value<NamedColumn(0), NamedColumn(2)> split_0;
    split_value<NamedColumn(2), NamedColumn(3)> split_1;
};

template <>
struct split_value<NamedColumn(0), NamedColumn(3)> {
    explicit split_value() = default;
    accessor<NamedColumn::value_1> value_1;
    accessor<NamedColumn::value_2a> value_2a;
    accessor<NamedColumn::value_2b> value_2b;
};
template <>
struct unified_value<NamedColumn(3)> {
    auto& value_1() {
        return split_0.value_1;
    }
    const auto& value_1() const {
        return split_0.value_1;
    }
    auto& value_2a() {
        return split_0.value_2a;
    }
    const auto& value_2a() const {
        return split_0.value_2a;
    }
    auto& value_2b() {
        return split_0.value_2b;
    }
    const auto& value_2b() const {
        return split_0.value_2b;
    }

    const auto split_of(NamedColumn index) const {
        return index < NamedColumn(3) ? 0 : 1;
    }

    split_value<NamedColumn(0), NamedColumn(3)> split_0;
};

struct index_value {
    using NamedColumn = index_value_datatypes::NamedColumn;
    static constexpr auto MAX_SPLITS = 2;

    explicit index_value() = default;

    static inline void resplit(
            index_value& newvalue, const index_value& oldvalue, NamedColumn index);

    template <NamedColumn Column>
    static inline void set_unified(index_value& value, NamedColumn index);

    template <NamedColumn Column>
    static inline void copy_data(index_value& newvalue, const index_value& oldvalue);

    template <NamedColumn Column>
    inline accessor<RoundedNamedColumn<Column>()>& get();

    template <NamedColumn Column>
    inline const accessor<RoundedNamedColumn<Column>()>& get() const;

    auto& value_1() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.value_1();
            }, value);
    }
    const auto& value_1() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.value_1();
            }, value);
    }
    auto& value_2a() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.value_2a();
            }, value);
    }
    const auto& value_2a() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.value_2a();
            }, value);
    }
    auto& value_2b() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.value_2b();
            }, value);
    }
    const auto& value_2b() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.value_2b();
            }, value);
    }

    const auto split_of(NamedColumn index) const {
        return std::visit([this, index] (auto&& val) -> const auto {
                return val.split_of(index);
            }, value);
    }

    std::variant<
        unified_value<NamedColumn(3)>,
        unified_value<NamedColumn(2)>,
        unified_value<NamedColumn(1)>
        > value;
};

inline void index_value::resplit(
        index_value& newvalue, const index_value& oldvalue, NamedColumn index) {
    assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
    set_unified<NamedColumn(1)>(newvalue, index);
    copy_data<NamedColumn(0)>(newvalue, oldvalue);
}

template <NamedColumn Column>
inline void index_value::set_unified(index_value& value, NamedColumn index) {
    static_assert(Column <= NamedColumn::COLCOUNT);
    if (Column == index) {
        value.value.emplace<unified_value<Column>>();
        return;
    }
    if constexpr (Column < NamedColumn::COLCOUNT) {
        set_unified<Column + 1>(value, index);
    }
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
inline accessor<RoundedNamedColumn<NamedColumn(0)>()>& index_value::get<NamedColumn(0)>() {
    return value_1();
}

template <>
inline const accessor<RoundedNamedColumn<NamedColumn(0)>()>& index_value::get<NamedColumn(0)>() const {
    return value_1();
}

template <>
inline accessor<RoundedNamedColumn<NamedColumn(1)>()>& index_value::get<NamedColumn(1)>() {
    return value_2a();
}

template <>
inline const accessor<RoundedNamedColumn<NamedColumn(1)>()>& index_value::get<NamedColumn(1)>() const {
    return value_2a();
}

template <>
inline accessor<RoundedNamedColumn<NamedColumn(2)>()>& index_value::get<NamedColumn(2)>() {
    return value_2b();
}

template <>
inline const accessor<RoundedNamedColumn<NamedColumn(2)>()>& index_value::get<NamedColumn(2)>() const {
    return value_2b();
}

};  // namespace index_value_datatypes

using index_value = index_value_datatypes::index_value;
using ADAPTER_OF(index_value) = ADAPTER_OF(index_value_datatypes::index_value);

