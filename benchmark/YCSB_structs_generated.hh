#pragma once

#include "Adapter.hh"
#include "Sto.hh"
#include "VersionSelector.hh"

namespace ycsb {

namespace ycsb_value_datatypes {

enum class NamedColumn : int {
    even_columns = 0,
    odd_columns = 5,
    COLCOUNT = 10
};

NamedColumn operator+(NamedColumn nc, std::underlying_type_t<NamedColumn> index) {
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
struct accessor;

template <NamedColumn StartIndex, NamedColumn EndIndex>
struct split_value;

template <NamedColumn SplitIndex>
struct unified_value;

struct ycsb_value;

DEFINE_ADAPTER(ycsb_value, NamedColumn);

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::even_columns> {
    using type = ::bench::fix_string<COL_WIDTH>;
};

template <>
struct accessor_info<NamedColumn::odd_columns> {
    using type = ::bench::fix_string<COL_WIDTH>;
};

template <NamedColumn Column>
struct accessor {
    using type = typename accessor_info<Column>::type;

    accessor() = default;
    accessor(type& value) : value_(value) {}
    accessor(const type& value) : value_(const_cast<type&>(value)) {}

    operator type() {
        ADAPTER_OF(ycsb_value)::CountWrite(Column + index_);
        return value_;
    }

    operator const type() const {
        ADAPTER_OF(ycsb_value)::CountRead(Column + index_);
        return value_;
    }

    operator type&() {
        ADAPTER_OF(ycsb_value)::CountWrite(Column + index_);
        return value_;
    }

    operator const type&() const {
        ADAPTER_OF(ycsb_value)::CountRead(Column + index_);
        return value_;
    }

    type operator =(const type& other) {
        ADAPTER_OF(ycsb_value)::CountWrite(Column + index_);
        return value_ = other;
    }

    type operator =(const accessor<Column>& other) {
        ADAPTER_OF(ycsb_value)::CountWrite(Column + index_);
        return value_ = other.value_;
    }

    type operator *() {
        ADAPTER_OF(ycsb_value)::CountWrite(Column + index_);
        return value_;
    }

    const type operator *() const {
        ADAPTER_OF(ycsb_value)::CountRead(Column + index_);
        return value_;
    }

    type* operator ->() {
        ADAPTER_OF(ycsb_value)::CountWrite(Column + index_);
        return &value_;
    }

    const type* operator ->() const {
        ADAPTER_OF(ycsb_value)::CountRead(Column + index_);
        return &value_;
    }

    std::underlying_type_t<NamedColumn> index_ = 0;
    type value_;
};

template <>
struct split_value<NamedColumn(0), NamedColumn(1)> {
    explicit split_value() {
        even_columns[0].index_ = 0;
    }
    std::array<accessor<NamedColumn::even_columns>, 1> even_columns;
};
template <>
struct split_value<NamedColumn(1), NamedColumn(10)> {
    explicit split_value() {
        even_columns[0].index_ = 1;
        even_columns[1].index_ = 2;
        even_columns[2].index_ = 3;
        even_columns[3].index_ = 4;
        odd_columns[0].index_ = 0;
        odd_columns[1].index_ = 1;
        odd_columns[2].index_ = 2;
        odd_columns[3].index_ = 3;
        odd_columns[4].index_ = 4;
    }
    std::array<accessor<NamedColumn::even_columns>, 4> even_columns;
    std::array<accessor<NamedColumn::odd_columns>, 5> odd_columns;
};
template <>
struct unified_value<NamedColumn(1)> {
    auto& even_columns(size_t index) {
        if (index < 1) {
            return split_0.even_columns[index];
        }
        return split_1.even_columns[index - 1];
    }
    const auto& even_columns(size_t index) const {
        if (index < 1) {
            return split_0.even_columns[index];
        }
        return split_1.even_columns[index - 1];
    }
    auto& odd_columns(size_t index) {
        return split_1.odd_columns[index];
    }
    const auto& odd_columns(size_t index) const {
        return split_1.odd_columns[index];
    }

    const auto split_of(NamedColumn index) const {
        return index < NamedColumn(1) ? 0 : 1;
    }

    split_value<NamedColumn(0), NamedColumn(1)> split_0;
    split_value<NamedColumn(1), NamedColumn(10)> split_1;
};

template <>
struct split_value<NamedColumn(0), NamedColumn(2)> {
    explicit split_value() {
        even_columns[0].index_ = 0;
        even_columns[1].index_ = 1;
    }
    std::array<accessor<NamedColumn::even_columns>, 2> even_columns;
};
template <>
struct split_value<NamedColumn(2), NamedColumn(10)> {
    explicit split_value() {
        even_columns[0].index_ = 2;
        even_columns[1].index_ = 3;
        even_columns[2].index_ = 4;
        odd_columns[0].index_ = 0;
        odd_columns[1].index_ = 1;
        odd_columns[2].index_ = 2;
        odd_columns[3].index_ = 3;
        odd_columns[4].index_ = 4;
    }
    std::array<accessor<NamedColumn::even_columns>, 3> even_columns;
    std::array<accessor<NamedColumn::odd_columns>, 5> odd_columns;
};
template <>
struct unified_value<NamedColumn(2)> {
    auto& even_columns(size_t index) {
        if (index < 2) {
            return split_0.even_columns[index];
        }
        return split_1.even_columns[index - 2];
    }
    const auto& even_columns(size_t index) const {
        if (index < 2) {
            return split_0.even_columns[index];
        }
        return split_1.even_columns[index - 2];
    }
    auto& odd_columns(size_t index) {
        return split_1.odd_columns[index];
    }
    const auto& odd_columns(size_t index) const {
        return split_1.odd_columns[index];
    }

    const auto split_of(NamedColumn index) const {
        return index < NamedColumn(2) ? 0 : 1;
    }

    split_value<NamedColumn(0), NamedColumn(2)> split_0;
    split_value<NamedColumn(2), NamedColumn(10)> split_1;
};

template <>
struct split_value<NamedColumn(0), NamedColumn(3)> {
    explicit split_value() {
        even_columns[0].index_ = 0;
        even_columns[1].index_ = 1;
        even_columns[2].index_ = 2;
    }
    std::array<accessor<NamedColumn::even_columns>, 3> even_columns;
};
template <>
struct split_value<NamedColumn(3), NamedColumn(10)> {
    explicit split_value() {
        even_columns[0].index_ = 3;
        even_columns[1].index_ = 4;
        odd_columns[0].index_ = 0;
        odd_columns[1].index_ = 1;
        odd_columns[2].index_ = 2;
        odd_columns[3].index_ = 3;
        odd_columns[4].index_ = 4;
    }
    std::array<accessor<NamedColumn::even_columns>, 2> even_columns;
    std::array<accessor<NamedColumn::odd_columns>, 5> odd_columns;
};
template <>
struct unified_value<NamedColumn(3)> {
    auto& even_columns(size_t index) {
        if (index < 3) {
            return split_0.even_columns[index];
        }
        return split_1.even_columns[index - 3];
    }
    const auto& even_columns(size_t index) const {
        if (index < 3) {
            return split_0.even_columns[index];
        }
        return split_1.even_columns[index - 3];
    }
    auto& odd_columns(size_t index) {
        return split_1.odd_columns[index];
    }
    const auto& odd_columns(size_t index) const {
        return split_1.odd_columns[index];
    }

    const auto split_of(NamedColumn index) const {
        return index < NamedColumn(3) ? 0 : 1;
    }

    split_value<NamedColumn(0), NamedColumn(3)> split_0;
    split_value<NamedColumn(3), NamedColumn(10)> split_1;
};

template <>
struct split_value<NamedColumn(0), NamedColumn(4)> {
    explicit split_value() {
        even_columns[0].index_ = 0;
        even_columns[1].index_ = 1;
        even_columns[2].index_ = 2;
        even_columns[3].index_ = 3;
    }
    std::array<accessor<NamedColumn::even_columns>, 4> even_columns;
};
template <>
struct split_value<NamedColumn(4), NamedColumn(10)> {
    explicit split_value() {
        even_columns[0].index_ = 4;
        odd_columns[0].index_ = 0;
        odd_columns[1].index_ = 1;
        odd_columns[2].index_ = 2;
        odd_columns[3].index_ = 3;
        odd_columns[4].index_ = 4;
    }
    std::array<accessor<NamedColumn::even_columns>, 1> even_columns;
    std::array<accessor<NamedColumn::odd_columns>, 5> odd_columns;
};
template <>
struct unified_value<NamedColumn(4)> {
    auto& even_columns(size_t index) {
        if (index < 4) {
            return split_0.even_columns[index];
        }
        return split_1.even_columns[index - 4];
    }
    const auto& even_columns(size_t index) const {
        if (index < 4) {
            return split_0.even_columns[index];
        }
        return split_1.even_columns[index - 4];
    }
    auto& odd_columns(size_t index) {
        return split_1.odd_columns[index];
    }
    const auto& odd_columns(size_t index) const {
        return split_1.odd_columns[index];
    }

    const auto split_of(NamedColumn index) const {
        return index < NamedColumn(4) ? 0 : 1;
    }

    split_value<NamedColumn(0), NamedColumn(4)> split_0;
    split_value<NamedColumn(4), NamedColumn(10)> split_1;
};

template <>
struct split_value<NamedColumn(0), NamedColumn(5)> {
    explicit split_value() {
        even_columns[0].index_ = 0;
        even_columns[1].index_ = 1;
        even_columns[2].index_ = 2;
        even_columns[3].index_ = 3;
        even_columns[4].index_ = 4;
    }
    std::array<accessor<NamedColumn::even_columns>, 5> even_columns;
};
template <>
struct split_value<NamedColumn(5), NamedColumn(10)> {
    explicit split_value() {
        odd_columns[0].index_ = 0;
        odd_columns[1].index_ = 1;
        odd_columns[2].index_ = 2;
        odd_columns[3].index_ = 3;
        odd_columns[4].index_ = 4;
    }
    std::array<accessor<NamedColumn::odd_columns>, 5> odd_columns;
};
template <>
struct unified_value<NamedColumn(5)> {
    auto& even_columns(size_t index) {
        return split_0.even_columns[index];
    }
    const auto& even_columns(size_t index) const {
        return split_0.even_columns[index];
    }
    auto& odd_columns(size_t index) {
        return split_1.odd_columns[index];
    }
    const auto& odd_columns(size_t index) const {
        return split_1.odd_columns[index];
    }

    const auto split_of(NamedColumn index) const {
        return index < NamedColumn(5) ? 0 : 1;
    }

    split_value<NamedColumn(0), NamedColumn(5)> split_0;
    split_value<NamedColumn(5), NamedColumn(10)> split_1;
};

template <>
struct split_value<NamedColumn(0), NamedColumn(6)> {
    explicit split_value() {
        even_columns[0].index_ = 0;
        even_columns[1].index_ = 1;
        even_columns[2].index_ = 2;
        even_columns[3].index_ = 3;
        even_columns[4].index_ = 4;
        odd_columns[0].index_ = 0;
    }
    std::array<accessor<NamedColumn::even_columns>, 5> even_columns;
    std::array<accessor<NamedColumn::odd_columns>, 1> odd_columns;
};
template <>
struct split_value<NamedColumn(6), NamedColumn(10)> {
    explicit split_value() {
        odd_columns[0].index_ = 1;
        odd_columns[1].index_ = 2;
        odd_columns[2].index_ = 3;
        odd_columns[3].index_ = 4;
    }
    std::array<accessor<NamedColumn::odd_columns>, 4> odd_columns;
};
template <>
struct unified_value<NamedColumn(6)> {
    auto& even_columns(size_t index) {
        return split_0.even_columns[index];
    }
    const auto& even_columns(size_t index) const {
        return split_0.even_columns[index];
    }
    auto& odd_columns(size_t index) {
        if (index < 6) {
            return split_0.odd_columns[index];
        }
        return split_1.odd_columns[index - 1];
    }
    const auto& odd_columns(size_t index) const {
        if (index < 6) {
            return split_0.odd_columns[index];
        }
        return split_1.odd_columns[index - 1];
    }

    const auto split_of(NamedColumn index) const {
        return index < NamedColumn(6) ? 0 : 1;
    }

    split_value<NamedColumn(0), NamedColumn(6)> split_0;
    split_value<NamedColumn(6), NamedColumn(10)> split_1;
};

template <>
struct split_value<NamedColumn(0), NamedColumn(7)> {
    explicit split_value() {
        even_columns[0].index_ = 0;
        even_columns[1].index_ = 1;
        even_columns[2].index_ = 2;
        even_columns[3].index_ = 3;
        even_columns[4].index_ = 4;
        odd_columns[0].index_ = 0;
        odd_columns[1].index_ = 1;
    }
    std::array<accessor<NamedColumn::even_columns>, 5> even_columns;
    std::array<accessor<NamedColumn::odd_columns>, 2> odd_columns;
};
template <>
struct split_value<NamedColumn(7), NamedColumn(10)> {
    explicit split_value() {
        odd_columns[0].index_ = 2;
        odd_columns[1].index_ = 3;
        odd_columns[2].index_ = 4;
    }
    std::array<accessor<NamedColumn::odd_columns>, 3> odd_columns;
};
template <>
struct unified_value<NamedColumn(7)> {
    auto& even_columns(size_t index) {
        return split_0.even_columns[index];
    }
    const auto& even_columns(size_t index) const {
        return split_0.even_columns[index];
    }
    auto& odd_columns(size_t index) {
        if (index < 7) {
            return split_0.odd_columns[index];
        }
        return split_1.odd_columns[index - 2];
    }
    const auto& odd_columns(size_t index) const {
        if (index < 7) {
            return split_0.odd_columns[index];
        }
        return split_1.odd_columns[index - 2];
    }

    const auto split_of(NamedColumn index) const {
        return index < NamedColumn(7) ? 0 : 1;
    }

    split_value<NamedColumn(0), NamedColumn(7)> split_0;
    split_value<NamedColumn(7), NamedColumn(10)> split_1;
};

template <>
struct split_value<NamedColumn(0), NamedColumn(8)> {
    explicit split_value() {
        even_columns[0].index_ = 0;
        even_columns[1].index_ = 1;
        even_columns[2].index_ = 2;
        even_columns[3].index_ = 3;
        even_columns[4].index_ = 4;
        odd_columns[0].index_ = 0;
        odd_columns[1].index_ = 1;
        odd_columns[2].index_ = 2;
    }
    std::array<accessor<NamedColumn::even_columns>, 5> even_columns;
    std::array<accessor<NamedColumn::odd_columns>, 3> odd_columns;
};
template <>
struct split_value<NamedColumn(8), NamedColumn(10)> {
    explicit split_value() {
        odd_columns[0].index_ = 3;
        odd_columns[1].index_ = 4;
    }
    std::array<accessor<NamedColumn::odd_columns>, 2> odd_columns;
};
template <>
struct unified_value<NamedColumn(8)> {
    auto& even_columns(size_t index) {
        return split_0.even_columns[index];
    }
    const auto& even_columns(size_t index) const {
        return split_0.even_columns[index];
    }
    auto& odd_columns(size_t index) {
        if (index < 8) {
            return split_0.odd_columns[index];
        }
        return split_1.odd_columns[index - 3];
    }
    const auto& odd_columns(size_t index) const {
        if (index < 8) {
            return split_0.odd_columns[index];
        }
        return split_1.odd_columns[index - 3];
    }

    const auto split_of(NamedColumn index) const {
        return index < NamedColumn(8) ? 0 : 1;
    }

    split_value<NamedColumn(0), NamedColumn(8)> split_0;
    split_value<NamedColumn(8), NamedColumn(10)> split_1;
};

template <>
struct split_value<NamedColumn(0), NamedColumn(9)> {
    explicit split_value() {
        even_columns[0].index_ = 0;
        even_columns[1].index_ = 1;
        even_columns[2].index_ = 2;
        even_columns[3].index_ = 3;
        even_columns[4].index_ = 4;
        odd_columns[0].index_ = 0;
        odd_columns[1].index_ = 1;
        odd_columns[2].index_ = 2;
        odd_columns[3].index_ = 3;
    }
    std::array<accessor<NamedColumn::even_columns>, 5> even_columns;
    std::array<accessor<NamedColumn::odd_columns>, 4> odd_columns;
};
template <>
struct split_value<NamedColumn(9), NamedColumn(10)> {
    explicit split_value() {
        odd_columns[0].index_ = 4;
    }
    std::array<accessor<NamedColumn::odd_columns>, 1> odd_columns;
};
template <>
struct unified_value<NamedColumn(9)> {
    auto& even_columns(size_t index) {
        return split_0.even_columns[index];
    }
    const auto& even_columns(size_t index) const {
        return split_0.even_columns[index];
    }
    auto& odd_columns(size_t index) {
        if (index < 9) {
            return split_0.odd_columns[index];
        }
        return split_1.odd_columns[index - 4];
    }
    const auto& odd_columns(size_t index) const {
        if (index < 9) {
            return split_0.odd_columns[index];
        }
        return split_1.odd_columns[index - 4];
    }

    const auto split_of(NamedColumn index) const {
        return index < NamedColumn(9) ? 0 : 1;
    }

    split_value<NamedColumn(0), NamedColumn(9)> split_0;
    split_value<NamedColumn(9), NamedColumn(10)> split_1;
};

template <>
struct split_value<NamedColumn(0), NamedColumn(10)> {
    explicit split_value() {
        even_columns[0].index_ = 0;
        even_columns[1].index_ = 1;
        even_columns[2].index_ = 2;
        even_columns[3].index_ = 3;
        even_columns[4].index_ = 4;
        odd_columns[0].index_ = 0;
        odd_columns[1].index_ = 1;
        odd_columns[2].index_ = 2;
        odd_columns[3].index_ = 3;
        odd_columns[4].index_ = 4;
    }
    std::array<accessor<NamedColumn::even_columns>, 5> even_columns;
    std::array<accessor<NamedColumn::odd_columns>, 5> odd_columns;
};
template <>
struct unified_value<NamedColumn(10)> {
    auto& even_columns(size_t index) {
        return split_0.even_columns[index];
    }
    const auto& even_columns(size_t index) const {
        return split_0.even_columns[index];
    }
    auto& odd_columns(size_t index) {
        return split_0.odd_columns[index];
    }
    const auto& odd_columns(size_t index) const {
        return split_0.odd_columns[index];
    }

    const auto split_of(NamedColumn index) const {
        return index < NamedColumn(10) ? 0 : 1;
    }

    split_value<NamedColumn(0), NamedColumn(10)> split_0;
};

struct ycsb_value {
    using NamedColumn = ycsb_value_datatypes::NamedColumn;
    static constexpr auto MAX_SPLITS = 2;

    explicit ycsb_value() = default;

    template <NamedColumn Column>
    inline auto& get();

    auto& even_columns(size_t index) {
        return std::visit([this, index] (auto&& val) -> auto& {
                return val.even_columns(index);
            }, value);
    }
    const auto& even_columns(size_t index) const {
        return std::visit([this, index] (auto&& val) -> const auto& {
                return val.even_columns(index);
            }, value);
    }
    auto& odd_columns(size_t index) {
        return std::visit([this, index] (auto&& val) -> auto& {
                return val.odd_columns(index);
            }, value);
    }
    const auto& odd_columns(size_t index) const {
        return std::visit([this, index] (auto&& val) -> const auto& {
                return val.odd_columns(index);
            }, value);
    }

    const auto split_of(NamedColumn index) const {
        return std::visit([this, index] (auto&& val) -> const auto {
                return val.split_of(index);
            }, value);
    }

    std::variant<
        unified_value<NamedColumn(10)>,
        unified_value<NamedColumn(9)>,
        unified_value<NamedColumn(8)>,
        unified_value<NamedColumn(7)>,
        unified_value<NamedColumn(6)>,
        unified_value<NamedColumn(5)>,
        unified_value<NamedColumn(4)>,
        unified_value<NamedColumn(3)>,
        unified_value<NamedColumn(2)>,
        unified_value<NamedColumn(1)>
        > value;
};

template <>
inline auto& ycsb_value::get<NamedColumn(0)>() {
    return even_columns(0);
}

template <>
inline auto& ycsb_value::get<NamedColumn(1)>() {
    return even_columns(1);
}

template <>
inline auto& ycsb_value::get<NamedColumn(2)>() {
    return even_columns(2);
}

template <>
inline auto& ycsb_value::get<NamedColumn(3)>() {
    return even_columns(3);
}

template <>
inline auto& ycsb_value::get<NamedColumn(4)>() {
    return even_columns(4);
}

template <>
inline auto& ycsb_value::get<NamedColumn(5)>() {
    return odd_columns(0);
}

template <>
inline auto& ycsb_value::get<NamedColumn(6)>() {
    return odd_columns(1);
}

template <>
inline auto& ycsb_value::get<NamedColumn(7)>() {
    return odd_columns(2);
}

template <>
inline auto& ycsb_value::get<NamedColumn(8)>() {
    return odd_columns(3);
}

template <>
inline auto& ycsb_value::get<NamedColumn(9)>() {
    return odd_columns(4);
}

};  // namespace ycsb_value_datatypes

using ycsb_value = ycsb_value_datatypes::ycsb_value;
using ADAPTER_OF(ycsb_value) = ADAPTER_OF(ycsb_value_datatypes::ycsb_value);

};  // namespace ycsb

