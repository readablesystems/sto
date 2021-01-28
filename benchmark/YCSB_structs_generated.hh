namespace ycsb_value_datatypes {

enum class NamedColumn : int {
    even_columns = 0,
    odd_columns,
    COLCOUNT
};

template <size_t ColIndex>
struct accessor;

template <size_t StartIndex, size_t EndIndex>
struct split_value;

template <size_t SplitIndex>
struct unified_value;

struct ycsb_value;

CREATE_ADAPTER(ycsb_value, 10);

template <size_t ColIndex>
struct accessor_info;

template <>
struct accessor_info<0> {
    using type = fix_string<COL_WIDTH>;
};

template <>
struct accessor_info<5> {
    using type = fix_string<COL_WIDTH>;
};

template <size_t ColIndex>
struct accessor {
    using type = typename accessor_info<ColIndex>::type;

    accessor() = default;
    accessor(type& value) : value_(value) {}
    accessor(const type& value) : value_(const_cast<type&>(value)) {}

    operator type() {
        ADAPTER_OF(ycsb_value)::CountRead(ColIndex + index_);
        return value_;
    }

    operator const type() const {
        ADAPTER_OF(ycsb_value)::CountRead(ColIndex + index_);
        return value_;
    }

    operator type&() {
        ADAPTER_OF(ycsb_value)::CountRead(ColIndex + index_);
        return value_;
    }

    operator const type&() const {
        ADAPTER_OF(ycsb_value)::CountRead(ColIndex + index_);
        return value_;
    }

    type operator =(const type& other) {
        ADAPTER_OF(ycsb_value)::CountWrite(ColIndex + index_);
        return value_ = other;
    }

    type operator =(const accessor<ColIndex>& other) {
        ADAPTER_OF(ycsb_value)::CountWrite(ColIndex + index_);
        return value_ = other.value_;
    }

    type operator *() {
        ADAPTER_OF(ycsb_value)::CountRead(ColIndex + index_);
        return value_;
    }

    const type operator *() const {
        ADAPTER_OF(ycsb_value)::CountRead(ColIndex + index_);
        return value_;
    }

    type* operator ->() {
        ADAPTER_OF(ycsb_value)::CountRead(ColIndex + index_);
        return &value_;
    }

    const type* operator ->() const {
        ADAPTER_OF(ycsb_value)::CountRead(ColIndex + index_);
        return &value_;
    }

    size_t index_ = 0;
    type value_;
};

template <>
struct split_value<0, 1> {
    explicit split_value() {
        even_columns[0].index_ = 0;
    }
    std::array<accessor<0>, 1> even_columns;
};
template <>
struct split_value<1, 10> {
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
    std::array<accessor<0>, 4> even_columns;
    std::array<accessor<5>, 5> odd_columns;
};
template <>
struct unified_value<1> {
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

    split_value<0, 1> split_0;
    split_value<1, 10> split_1;
};

template <>
struct split_value<0, 2> {
    explicit split_value() {
        even_columns[0].index_ = 0;
        even_columns[1].index_ = 1;
    }
    std::array<accessor<0>, 2> even_columns;
};
template <>
struct split_value<2, 10> {
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
    std::array<accessor<0>, 3> even_columns;
    std::array<accessor<5>, 5> odd_columns;
};
template <>
struct unified_value<2> {
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

    split_value<0, 2> split_0;
    split_value<2, 10> split_1;
};

template <>
struct split_value<0, 3> {
    explicit split_value() {
        even_columns[0].index_ = 0;
        even_columns[1].index_ = 1;
        even_columns[2].index_ = 2;
    }
    std::array<accessor<0>, 3> even_columns;
};
template <>
struct split_value<3, 10> {
    explicit split_value() {
        even_columns[0].index_ = 3;
        even_columns[1].index_ = 4;
        odd_columns[0].index_ = 0;
        odd_columns[1].index_ = 1;
        odd_columns[2].index_ = 2;
        odd_columns[3].index_ = 3;
        odd_columns[4].index_ = 4;
    }
    std::array<accessor<0>, 2> even_columns;
    std::array<accessor<5>, 5> odd_columns;
};
template <>
struct unified_value<3> {
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

    split_value<0, 3> split_0;
    split_value<3, 10> split_1;
};

template <>
struct split_value<0, 4> {
    explicit split_value() {
        even_columns[0].index_ = 0;
        even_columns[1].index_ = 1;
        even_columns[2].index_ = 2;
        even_columns[3].index_ = 3;
    }
    std::array<accessor<0>, 4> even_columns;
};
template <>
struct split_value<4, 10> {
    explicit split_value() {
        even_columns[0].index_ = 4;
        odd_columns[0].index_ = 0;
        odd_columns[1].index_ = 1;
        odd_columns[2].index_ = 2;
        odd_columns[3].index_ = 3;
        odd_columns[4].index_ = 4;
    }
    std::array<accessor<0>, 1> even_columns;
    std::array<accessor<5>, 5> odd_columns;
};
template <>
struct unified_value<4> {
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

    split_value<0, 4> split_0;
    split_value<4, 10> split_1;
};

template <>
struct split_value<0, 5> {
    explicit split_value() {
        even_columns[0].index_ = 0;
        even_columns[1].index_ = 1;
        even_columns[2].index_ = 2;
        even_columns[3].index_ = 3;
        even_columns[4].index_ = 4;
    }
    std::array<accessor<0>, 5> even_columns;
};
template <>
struct split_value<5, 10> {
    explicit split_value() {
        odd_columns[0].index_ = 0;
        odd_columns[1].index_ = 1;
        odd_columns[2].index_ = 2;
        odd_columns[3].index_ = 3;
        odd_columns[4].index_ = 4;
    }
    std::array<accessor<5>, 5> odd_columns;
};
template <>
struct unified_value<5> {
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

    split_value<0, 5> split_0;
    split_value<5, 10> split_1;
};

template <>
struct split_value<0, 6> {
    explicit split_value() {
        even_columns[0].index_ = 0;
        even_columns[1].index_ = 1;
        even_columns[2].index_ = 2;
        even_columns[3].index_ = 3;
        even_columns[4].index_ = 4;
        odd_columns[0].index_ = 0;
    }
    std::array<accessor<0>, 5> even_columns;
    std::array<accessor<5>, 1> odd_columns;
};
template <>
struct split_value<6, 10> {
    explicit split_value() {
        odd_columns[0].index_ = 1;
        odd_columns[1].index_ = 2;
        odd_columns[2].index_ = 3;
        odd_columns[3].index_ = 4;
    }
    std::array<accessor<5>, 4> odd_columns;
};
template <>
struct unified_value<6> {
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

    split_value<0, 6> split_0;
    split_value<6, 10> split_1;
};

template <>
struct split_value<0, 7> {
    explicit split_value() {
        even_columns[0].index_ = 0;
        even_columns[1].index_ = 1;
        even_columns[2].index_ = 2;
        even_columns[3].index_ = 3;
        even_columns[4].index_ = 4;
        odd_columns[0].index_ = 0;
        odd_columns[1].index_ = 1;
    }
    std::array<accessor<0>, 5> even_columns;
    std::array<accessor<5>, 2> odd_columns;
};
template <>
struct split_value<7, 10> {
    explicit split_value() {
        odd_columns[0].index_ = 2;
        odd_columns[1].index_ = 3;
        odd_columns[2].index_ = 4;
    }
    std::array<accessor<5>, 3> odd_columns;
};
template <>
struct unified_value<7> {
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

    split_value<0, 7> split_0;
    split_value<7, 10> split_1;
};

template <>
struct split_value<0, 8> {
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
    std::array<accessor<0>, 5> even_columns;
    std::array<accessor<5>, 3> odd_columns;
};
template <>
struct split_value<8, 10> {
    explicit split_value() {
        odd_columns[0].index_ = 3;
        odd_columns[1].index_ = 4;
    }
    std::array<accessor<5>, 2> odd_columns;
};
template <>
struct unified_value<8> {
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

    split_value<0, 8> split_0;
    split_value<8, 10> split_1;
};

template <>
struct split_value<0, 9> {
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
    std::array<accessor<0>, 5> even_columns;
    std::array<accessor<5>, 4> odd_columns;
};
template <>
struct split_value<9, 10> {
    explicit split_value() {
        odd_columns[0].index_ = 4;
    }
    std::array<accessor<5>, 1> odd_columns;
};
template <>
struct unified_value<9> {
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

    split_value<0, 9> split_0;
    split_value<9, 10> split_1;
};

template <>
struct split_value<0, 10> {
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
    std::array<accessor<0>, 5> even_columns;
    std::array<accessor<5>, 5> odd_columns;
};
template <>
struct unified_value<10> {
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

    split_value<0, 10> split_0;
};

struct ycsb_value {
    explicit ycsb_value() = default;

    using NamedColumn = ycsb_value_datatypes::NamedColumn;

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

    std::variant<
        unified_value<10>,
        unified_value<9>,
        unified_value<8>,
        unified_value<7>,
        unified_value<6>,
        unified_value<5>,
        unified_value<4>,
        unified_value<3>,
        unified_value<2>,
        unified_value<1>
        > value;
};
};  // namespace ycsb_value_datatypes

using ycsb_value = ycsb_value_datatypes::ycsb_value;
using ADAPTER_OF(ycsb_value) = ADAPTER_OF(ycsb_value_datatypes::ycsb_value);

