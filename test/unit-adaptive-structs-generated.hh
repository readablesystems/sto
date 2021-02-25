#pragma once

#include "Adapter.hh"
#include "Sto.hh"
#include "VersionSelector.hh"

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

template <NamedColumn StartIndex, NamedColumn EndIndex>
struct split_value;

template <NamedColumn SplitIndex>
struct unified_value;

struct index_value;

DEFINE_ADAPTER(index_value, NamedColumn);

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::data> {
    using type = double;
};

template <>
struct accessor_info<NamedColumn::label> {
    using type = std::string;
};

template <>
struct accessor_info<NamedColumn::flagged> {
    using type = bool;
};

template <NamedColumn Column>
struct accessor {
    using type = typename accessor_info<Column>::type;

    accessor() = default;
    accessor(type& value) : value_(value) {}
    accessor(const type& value) : value_(const_cast<type&>(value)) {}

    explicit operator type() {
        ADAPTER_OF(index_value)::CountWrite(Column + index_);
        return value_;
    }

    explicit operator const type() const {
        ADAPTER_OF(index_value)::CountRead(Column + index_);
        return value_;
    }

    explicit operator type&() {
        ADAPTER_OF(index_value)::CountWrite(Column + index_);
        return value_;
    }

    explicit operator const type&() const {
        ADAPTER_OF(index_value)::CountRead(Column + index_);
        return value_;
    }

    type& operator =(type& other) {
        ADAPTER_OF(index_value)::CountRead(Column + index_);
        printf("ASDF1\n");
        return other = value_;
    }

    accessor<Column>& operator =(const type& other) {
        ADAPTER_OF(index_value)::CountWrite(Column + index_);
        printf("ASDF2\n");
        value_ = other;
        return *this;
    }

    accessor<Column>& operator =(const accessor<Column>& other) {
        ADAPTER_OF(index_value)::CountWrite(Column + index_);
        ADAPTER_OF(index_value)::CountRead(Column + other.index_);
        printf("ASDF3\n");
        value_ = other.value_;
        return *this;
    }

    /*
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
    */

    std::underlying_type_t<NamedColumn> index_ = 0;
    type value_;
};

template <>
struct split_value<NamedColumn(0), NamedColumn(1)> {
    explicit split_value() {
        data[0].index_ = 0;
    }
    std::array<accessor<NamedColumn::data>, 1> data;
};
template <>
struct split_value<NamedColumn(1), NamedColumn(4)> {
    explicit split_value() {
        data[0].index_ = 1;
    }
    std::array<accessor<NamedColumn::data>, 1> data;
    accessor<NamedColumn::label> label;
    accessor<NamedColumn::flagged> flagged;
};
template <>
struct unified_value<NamedColumn(1)> {
    auto& data(size_t index) {
        if (index < 1) {
            return split_0.data[index];
        }
        return split_1.data[index - 1];
    }
    const auto& data(size_t index) const {
        if (index < 1) {
            return split_0.data[index];
        }
        return split_1.data[index - 1];
    }
    auto& label() {
        return split_1.label;
    }
    const auto& label() const {
        return split_1.label;
    }
    auto& flagged() {
        return split_1.flagged;
    }
    const auto& flagged() const {
        return split_1.flagged;
    }

    const auto split_of(NamedColumn index) const {
        return index < NamedColumn(1) ? 0 : 1;
    }

    split_value<NamedColumn(0), NamedColumn(1)> split_0;
    split_value<NamedColumn(1), NamedColumn(4)> split_1;
};

template <>
struct split_value<NamedColumn(0), NamedColumn(2)> {
    explicit split_value() {
        data[0].index_ = 0;
        data[1].index_ = 1;
    }
    std::array<accessor<NamedColumn::data>, 2> data;
};
template <>
struct split_value<NamedColumn(2), NamedColumn(4)> {
    explicit split_value() = default;
    accessor<NamedColumn::label> label;
    accessor<NamedColumn::flagged> flagged;
};
template <>
struct unified_value<NamedColumn(2)> {
    auto& data(size_t index) {
        return split_0.data[index];
    }
    const auto& data(size_t index) const {
        return split_0.data[index];
    }
    auto& label() {
        return split_1.label;
    }
    const auto& label() const {
        return split_1.label;
    }
    auto& flagged() {
        return split_1.flagged;
    }
    const auto& flagged() const {
        return split_1.flagged;
    }

    const auto split_of(NamedColumn index) const {
        return index < NamedColumn(2) ? 0 : 1;
    }

    split_value<NamedColumn(0), NamedColumn(2)> split_0;
    split_value<NamedColumn(2), NamedColumn(4)> split_1;
};

template <>
struct split_value<NamedColumn(0), NamedColumn(3)> {
    explicit split_value() {
        data[0].index_ = 0;
        data[1].index_ = 1;
    }
    std::array<accessor<NamedColumn::data>, 2> data;
    accessor<NamedColumn::label> label;
};
template <>
struct split_value<NamedColumn(3), NamedColumn(4)> {
    explicit split_value() = default;
    accessor<NamedColumn::flagged> flagged;
};
template <>
struct unified_value<NamedColumn(3)> {
    auto& data(size_t index) {
        return split_0.data[index];
    }
    const auto& data(size_t index) const {
        return split_0.data[index];
    }
    auto& label() {
        return split_0.label;
    }
    const auto& label() const {
        return split_0.label;
    }
    auto& flagged() {
        return split_1.flagged;
    }
    const auto& flagged() const {
        return split_1.flagged;
    }

    const auto split_of(NamedColumn index) const {
        return index < NamedColumn(3) ? 0 : 1;
    }

    split_value<NamedColumn(0), NamedColumn(3)> split_0;
    split_value<NamedColumn(3), NamedColumn(4)> split_1;
};

template <>
struct split_value<NamedColumn(0), NamedColumn(4)> {
    explicit split_value() {
        data[0].index_ = 0;
        data[1].index_ = 1;
    }
    std::array<accessor<NamedColumn::data>, 2> data;
    accessor<NamedColumn::label> label;
    accessor<NamedColumn::flagged> flagged;
};
template <>
struct unified_value<NamedColumn(4)> {
    auto& data(size_t index) {
        return split_0.data[index];
    }
    const auto& data(size_t index) const {
        return split_0.data[index];
    }
    auto& label() {
        return split_0.label;
    }
    const auto& label() const {
        return split_0.label;
    }
    auto& flagged() {
        return split_0.flagged;
    }
    const auto& flagged() const {
        return split_0.flagged;
    }

    const auto split_of(NamedColumn index) const {
        return index < NamedColumn(4) ? 0 : 1;
    }

    split_value<NamedColumn(0), NamedColumn(4)> split_0;
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

    auto& data(size_t index) {
        return std::visit([this, index] (auto&& val) -> auto& {
                return val.data(index);
            }, value);
    }
    const auto& data(size_t index) const {
        return std::visit([this, index] (auto&& val) -> const auto& {
                return val.data(index);
            }, value);
    }
    auto& label() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.label();
            }, value);
    }
    const auto& label() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.label();
            }, value);
    }
    auto& flagged() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.flagged();
            }, value);
    }
    const auto& flagged() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.flagged();
            }, value);
    }

    const auto split_of(NamedColumn index) const {
        return std::visit([this, index] (auto&& val) -> const auto {
                return val.split_of(index);
            }, value);
    }

    std::variant<
        unified_value<NamedColumn(4)>,
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
    return data(0);
}

template <>
inline const accessor<RoundedNamedColumn<NamedColumn(0)>()>& index_value::get<NamedColumn(0)>() const {
    return data(0);
}

template <>
inline accessor<RoundedNamedColumn<NamedColumn(0)>()>& index_value::get<NamedColumn(1)>() {
    return data(1);
}

template <>
inline const accessor<RoundedNamedColumn<NamedColumn(0)>()>& index_value::get<NamedColumn(1)>() const {
    return data(1);
}

template <>
inline accessor<RoundedNamedColumn<NamedColumn(2)>()>& index_value::get<NamedColumn(2)>() {
    return label();
}

template <>
inline const accessor<RoundedNamedColumn<NamedColumn(2)>()>& index_value::get<NamedColumn(2)>() const {
    return label();
}

template <>
inline accessor<RoundedNamedColumn<NamedColumn(3)>()>& index_value::get<NamedColumn(3)>() {
    return flagged();
}

template <>
inline const accessor<RoundedNamedColumn<NamedColumn(3)>()>& index_value::get<NamedColumn(3)>() const {
    return flagged();
}

};  // namespace index_value_datatypes

using index_value = index_value_datatypes::index_value;
using ADAPTER_OF(index_value) = ADAPTER_OF(index_value_datatypes::index_value);

