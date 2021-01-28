namespace warehouse_value_datatypes {

enum class NamedColumn : int {
    w_name = 0,
    w_street_1 = 1,
    w_street_2 = 2,
    w_city = 3,
    w_state = 4,
    w_zip = 5,
    w_tax = 6,
    w_ytd = 7,
    COLCOUNT = 8
};

template <size_t ColIndex>
struct accessor;

template <size_t StartIndex, size_t EndIndex>
struct split_value;

template <size_t SplitIndex>
struct unified_value;

struct warehouse_value;

DEFINE_ADAPTER(warehouse_value, 8);

template <size_t ColIndex>
struct accessor_info;

template <>
struct accessor_info<0> {
    using type = var_string<10>;
};

template <>
struct accessor_info<1> {
    using type = var_string<20>;
};

template <>
struct accessor_info<2> {
    using type = var_string<20>;
};

template <>
struct accessor_info<3> {
    using type = var_string<20>;
};

template <>
struct accessor_info<4> {
    using type = fix_string<2>;
};

template <>
struct accessor_info<5> {
    using type = fix_string<9>;
};

template <>
struct accessor_info<6> {
    using type = int64_t;
};

template <>
struct accessor_info<7> {
    using type = uint64_t;
};

template <size_t ColIndex>
struct accessor {
    using type = typename accessor_info<ColIndex>::type;

    accessor() = default;
    accessor(type& value) : value_(value) {}
    accessor(const type& value) : value_(const_cast<type&>(value)) {}

    operator type() {
        ADAPTER_OF(warehouse_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    operator const type() const {
        ADAPTER_OF(warehouse_value)::CountRead(ColIndex + index_);
        return value_;
    }

    operator type&() {
        ADAPTER_OF(warehouse_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    operator const type&() const {
        ADAPTER_OF(warehouse_value)::CountRead(ColIndex + index_);
        return value_;
    }

    type operator =(const type& other) {
        ADAPTER_OF(warehouse_value)::CountWrite(ColIndex + index_);
        return value_ = other;
    }

    type operator =(const accessor<ColIndex>& other) {
        ADAPTER_OF(warehouse_value)::CountWrite(ColIndex + index_);
        return value_ = other.value_;
    }

    type operator *() {
        ADAPTER_OF(warehouse_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    const type operator *() const {
        ADAPTER_OF(warehouse_value)::CountRead(ColIndex + index_);
        return value_;
    }

    type* operator ->() {
        ADAPTER_OF(warehouse_value)::CountWrite(ColIndex + index_);
        return &value_;
    }

    const type* operator ->() const {
        ADAPTER_OF(warehouse_value)::CountRead(ColIndex + index_);
        return &value_;
    }

    size_t index_ = 0;
    type value_;
};

template <>
struct split_value<0, 1> {
    explicit split_value() = default;
    accessor<0> w_name;
};
template <>
struct split_value<1, 8> {
    explicit split_value() = default;
    accessor<1> w_street_1;
    accessor<2> w_street_2;
    accessor<3> w_city;
    accessor<4> w_state;
    accessor<5> w_zip;
    accessor<6> w_tax;
    accessor<7> w_ytd;
};
template <>
struct unified_value<1> {
    auto& w_name() {
        return split_0.w_name;
    }
    const auto& w_name() const {
        return split_0.w_name;
    }
    auto& w_street_1() {
        return split_1.w_street_1;
    }
    const auto& w_street_1() const {
        return split_1.w_street_1;
    }
    auto& w_street_2() {
        return split_1.w_street_2;
    }
    const auto& w_street_2() const {
        return split_1.w_street_2;
    }
    auto& w_city() {
        return split_1.w_city;
    }
    const auto& w_city() const {
        return split_1.w_city;
    }
    auto& w_state() {
        return split_1.w_state;
    }
    const auto& w_state() const {
        return split_1.w_state;
    }
    auto& w_zip() {
        return split_1.w_zip;
    }
    const auto& w_zip() const {
        return split_1.w_zip;
    }
    auto& w_tax() {
        return split_1.w_tax;
    }
    const auto& w_tax() const {
        return split_1.w_tax;
    }
    auto& w_ytd() {
        return split_1.w_ytd;
    }
    const auto& w_ytd() const {
        return split_1.w_ytd;
    }

    split_value<0, 1> split_0;
    split_value<1, 8> split_1;
};

template <>
struct split_value<0, 2> {
    explicit split_value() = default;
    accessor<0> w_name;
    accessor<1> w_street_1;
};
template <>
struct split_value<2, 8> {
    explicit split_value() = default;
    accessor<2> w_street_2;
    accessor<3> w_city;
    accessor<4> w_state;
    accessor<5> w_zip;
    accessor<6> w_tax;
    accessor<7> w_ytd;
};
template <>
struct unified_value<2> {
    auto& w_name() {
        return split_0.w_name;
    }
    const auto& w_name() const {
        return split_0.w_name;
    }
    auto& w_street_1() {
        return split_0.w_street_1;
    }
    const auto& w_street_1() const {
        return split_0.w_street_1;
    }
    auto& w_street_2() {
        return split_1.w_street_2;
    }
    const auto& w_street_2() const {
        return split_1.w_street_2;
    }
    auto& w_city() {
        return split_1.w_city;
    }
    const auto& w_city() const {
        return split_1.w_city;
    }
    auto& w_state() {
        return split_1.w_state;
    }
    const auto& w_state() const {
        return split_1.w_state;
    }
    auto& w_zip() {
        return split_1.w_zip;
    }
    const auto& w_zip() const {
        return split_1.w_zip;
    }
    auto& w_tax() {
        return split_1.w_tax;
    }
    const auto& w_tax() const {
        return split_1.w_tax;
    }
    auto& w_ytd() {
        return split_1.w_ytd;
    }
    const auto& w_ytd() const {
        return split_1.w_ytd;
    }

    split_value<0, 2> split_0;
    split_value<2, 8> split_1;
};

template <>
struct split_value<0, 3> {
    explicit split_value() = default;
    accessor<0> w_name;
    accessor<1> w_street_1;
    accessor<2> w_street_2;
};
template <>
struct split_value<3, 8> {
    explicit split_value() = default;
    accessor<3> w_city;
    accessor<4> w_state;
    accessor<5> w_zip;
    accessor<6> w_tax;
    accessor<7> w_ytd;
};
template <>
struct unified_value<3> {
    auto& w_name() {
        return split_0.w_name;
    }
    const auto& w_name() const {
        return split_0.w_name;
    }
    auto& w_street_1() {
        return split_0.w_street_1;
    }
    const auto& w_street_1() const {
        return split_0.w_street_1;
    }
    auto& w_street_2() {
        return split_0.w_street_2;
    }
    const auto& w_street_2() const {
        return split_0.w_street_2;
    }
    auto& w_city() {
        return split_1.w_city;
    }
    const auto& w_city() const {
        return split_1.w_city;
    }
    auto& w_state() {
        return split_1.w_state;
    }
    const auto& w_state() const {
        return split_1.w_state;
    }
    auto& w_zip() {
        return split_1.w_zip;
    }
    const auto& w_zip() const {
        return split_1.w_zip;
    }
    auto& w_tax() {
        return split_1.w_tax;
    }
    const auto& w_tax() const {
        return split_1.w_tax;
    }
    auto& w_ytd() {
        return split_1.w_ytd;
    }
    const auto& w_ytd() const {
        return split_1.w_ytd;
    }

    split_value<0, 3> split_0;
    split_value<3, 8> split_1;
};

template <>
struct split_value<0, 4> {
    explicit split_value() = default;
    accessor<0> w_name;
    accessor<1> w_street_1;
    accessor<2> w_street_2;
    accessor<3> w_city;
};
template <>
struct split_value<4, 8> {
    explicit split_value() = default;
    accessor<4> w_state;
    accessor<5> w_zip;
    accessor<6> w_tax;
    accessor<7> w_ytd;
};
template <>
struct unified_value<4> {
    auto& w_name() {
        return split_0.w_name;
    }
    const auto& w_name() const {
        return split_0.w_name;
    }
    auto& w_street_1() {
        return split_0.w_street_1;
    }
    const auto& w_street_1() const {
        return split_0.w_street_1;
    }
    auto& w_street_2() {
        return split_0.w_street_2;
    }
    const auto& w_street_2() const {
        return split_0.w_street_2;
    }
    auto& w_city() {
        return split_0.w_city;
    }
    const auto& w_city() const {
        return split_0.w_city;
    }
    auto& w_state() {
        return split_1.w_state;
    }
    const auto& w_state() const {
        return split_1.w_state;
    }
    auto& w_zip() {
        return split_1.w_zip;
    }
    const auto& w_zip() const {
        return split_1.w_zip;
    }
    auto& w_tax() {
        return split_1.w_tax;
    }
    const auto& w_tax() const {
        return split_1.w_tax;
    }
    auto& w_ytd() {
        return split_1.w_ytd;
    }
    const auto& w_ytd() const {
        return split_1.w_ytd;
    }

    split_value<0, 4> split_0;
    split_value<4, 8> split_1;
};

template <>
struct split_value<0, 5> {
    explicit split_value() = default;
    accessor<0> w_name;
    accessor<1> w_street_1;
    accessor<2> w_street_2;
    accessor<3> w_city;
    accessor<4> w_state;
};
template <>
struct split_value<5, 8> {
    explicit split_value() = default;
    accessor<5> w_zip;
    accessor<6> w_tax;
    accessor<7> w_ytd;
};
template <>
struct unified_value<5> {
    auto& w_name() {
        return split_0.w_name;
    }
    const auto& w_name() const {
        return split_0.w_name;
    }
    auto& w_street_1() {
        return split_0.w_street_1;
    }
    const auto& w_street_1() const {
        return split_0.w_street_1;
    }
    auto& w_street_2() {
        return split_0.w_street_2;
    }
    const auto& w_street_2() const {
        return split_0.w_street_2;
    }
    auto& w_city() {
        return split_0.w_city;
    }
    const auto& w_city() const {
        return split_0.w_city;
    }
    auto& w_state() {
        return split_0.w_state;
    }
    const auto& w_state() const {
        return split_0.w_state;
    }
    auto& w_zip() {
        return split_1.w_zip;
    }
    const auto& w_zip() const {
        return split_1.w_zip;
    }
    auto& w_tax() {
        return split_1.w_tax;
    }
    const auto& w_tax() const {
        return split_1.w_tax;
    }
    auto& w_ytd() {
        return split_1.w_ytd;
    }
    const auto& w_ytd() const {
        return split_1.w_ytd;
    }

    split_value<0, 5> split_0;
    split_value<5, 8> split_1;
};

template <>
struct split_value<0, 6> {
    explicit split_value() = default;
    accessor<0> w_name;
    accessor<1> w_street_1;
    accessor<2> w_street_2;
    accessor<3> w_city;
    accessor<4> w_state;
    accessor<5> w_zip;
};
template <>
struct split_value<6, 8> {
    explicit split_value() = default;
    accessor<6> w_tax;
    accessor<7> w_ytd;
};
template <>
struct unified_value<6> {
    auto& w_name() {
        return split_0.w_name;
    }
    const auto& w_name() const {
        return split_0.w_name;
    }
    auto& w_street_1() {
        return split_0.w_street_1;
    }
    const auto& w_street_1() const {
        return split_0.w_street_1;
    }
    auto& w_street_2() {
        return split_0.w_street_2;
    }
    const auto& w_street_2() const {
        return split_0.w_street_2;
    }
    auto& w_city() {
        return split_0.w_city;
    }
    const auto& w_city() const {
        return split_0.w_city;
    }
    auto& w_state() {
        return split_0.w_state;
    }
    const auto& w_state() const {
        return split_0.w_state;
    }
    auto& w_zip() {
        return split_0.w_zip;
    }
    const auto& w_zip() const {
        return split_0.w_zip;
    }
    auto& w_tax() {
        return split_1.w_tax;
    }
    const auto& w_tax() const {
        return split_1.w_tax;
    }
    auto& w_ytd() {
        return split_1.w_ytd;
    }
    const auto& w_ytd() const {
        return split_1.w_ytd;
    }

    split_value<0, 6> split_0;
    split_value<6, 8> split_1;
};

template <>
struct split_value<0, 7> {
    explicit split_value() = default;
    accessor<0> w_name;
    accessor<1> w_street_1;
    accessor<2> w_street_2;
    accessor<3> w_city;
    accessor<4> w_state;
    accessor<5> w_zip;
    accessor<6> w_tax;
};
template <>
struct split_value<7, 8> {
    explicit split_value() = default;
    accessor<7> w_ytd;
};
template <>
struct unified_value<7> {
    auto& w_name() {
        return split_0.w_name;
    }
    const auto& w_name() const {
        return split_0.w_name;
    }
    auto& w_street_1() {
        return split_0.w_street_1;
    }
    const auto& w_street_1() const {
        return split_0.w_street_1;
    }
    auto& w_street_2() {
        return split_0.w_street_2;
    }
    const auto& w_street_2() const {
        return split_0.w_street_2;
    }
    auto& w_city() {
        return split_0.w_city;
    }
    const auto& w_city() const {
        return split_0.w_city;
    }
    auto& w_state() {
        return split_0.w_state;
    }
    const auto& w_state() const {
        return split_0.w_state;
    }
    auto& w_zip() {
        return split_0.w_zip;
    }
    const auto& w_zip() const {
        return split_0.w_zip;
    }
    auto& w_tax() {
        return split_0.w_tax;
    }
    const auto& w_tax() const {
        return split_0.w_tax;
    }
    auto& w_ytd() {
        return split_1.w_ytd;
    }
    const auto& w_ytd() const {
        return split_1.w_ytd;
    }

    split_value<0, 7> split_0;
    split_value<7, 8> split_1;
};

template <>
struct split_value<0, 8> {
    explicit split_value() = default;
    accessor<0> w_name;
    accessor<1> w_street_1;
    accessor<2> w_street_2;
    accessor<3> w_city;
    accessor<4> w_state;
    accessor<5> w_zip;
    accessor<6> w_tax;
    accessor<7> w_ytd;
};
template <>
struct unified_value<8> {
    auto& w_name() {
        return split_0.w_name;
    }
    const auto& w_name() const {
        return split_0.w_name;
    }
    auto& w_street_1() {
        return split_0.w_street_1;
    }
    const auto& w_street_1() const {
        return split_0.w_street_1;
    }
    auto& w_street_2() {
        return split_0.w_street_2;
    }
    const auto& w_street_2() const {
        return split_0.w_street_2;
    }
    auto& w_city() {
        return split_0.w_city;
    }
    const auto& w_city() const {
        return split_0.w_city;
    }
    auto& w_state() {
        return split_0.w_state;
    }
    const auto& w_state() const {
        return split_0.w_state;
    }
    auto& w_zip() {
        return split_0.w_zip;
    }
    const auto& w_zip() const {
        return split_0.w_zip;
    }
    auto& w_tax() {
        return split_0.w_tax;
    }
    const auto& w_tax() const {
        return split_0.w_tax;
    }
    auto& w_ytd() {
        return split_0.w_ytd;
    }
    const auto& w_ytd() const {
        return split_0.w_ytd;
    }

    split_value<0, 8> split_0;
};

struct warehouse_value {
    explicit warehouse_value() = default;

    using NamedColumn = warehouse_value_datatypes::NamedColumn;

    auto& w_name() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.w_name();
            }, value);
    }
    const auto& w_name() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.w_name();
            }, value);
    }
    auto& w_street_1() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.w_street_1();
            }, value);
    }
    const auto& w_street_1() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.w_street_1();
            }, value);
    }
    auto& w_street_2() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.w_street_2();
            }, value);
    }
    const auto& w_street_2() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.w_street_2();
            }, value);
    }
    auto& w_city() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.w_city();
            }, value);
    }
    const auto& w_city() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.w_city();
            }, value);
    }
    auto& w_state() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.w_state();
            }, value);
    }
    const auto& w_state() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.w_state();
            }, value);
    }
    auto& w_zip() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.w_zip();
            }, value);
    }
    const auto& w_zip() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.w_zip();
            }, value);
    }
    auto& w_tax() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.w_tax();
            }, value);
    }
    const auto& w_tax() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.w_tax();
            }, value);
    }
    auto& w_ytd() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.w_ytd();
            }, value);
    }
    const auto& w_ytd() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.w_ytd();
            }, value);
    }

    std::variant<
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
};  // namespace warehouse_value_datatypes

using warehouse_value = warehouse_value_datatypes::warehouse_value;
using ADAPTER_OF(warehouse_value) = ADAPTER_OF(warehouse_value_datatypes::warehouse_value);

namespace district_value_datatypes {

enum class NamedColumn : int {
    d_name = 0,
    d_street_1 = 1,
    d_street_2 = 2,
    d_city = 3,
    d_state = 4,
    d_zip = 5,
    d_tax = 6,
    d_ytd = 7,
    COLCOUNT = 8
};

template <size_t ColIndex>
struct accessor;

template <size_t StartIndex, size_t EndIndex>
struct split_value;

template <size_t SplitIndex>
struct unified_value;

struct district_value;

DEFINE_ADAPTER(district_value, 8);

template <size_t ColIndex>
struct accessor_info;

template <>
struct accessor_info<0> {
    using type = var_string<10>;
};

template <>
struct accessor_info<1> {
    using type = var_string<20>;
};

template <>
struct accessor_info<2> {
    using type = var_string<20>;
};

template <>
struct accessor_info<3> {
    using type = var_string<20>;
};

template <>
struct accessor_info<4> {
    using type = fix_string<2>;
};

template <>
struct accessor_info<5> {
    using type = fix_string<9>;
};

template <>
struct accessor_info<6> {
    using type = int64_t;
};

template <>
struct accessor_info<7> {
    using type = int64_t;
};

template <size_t ColIndex>
struct accessor {
    using type = typename accessor_info<ColIndex>::type;

    accessor() = default;
    accessor(type& value) : value_(value) {}
    accessor(const type& value) : value_(const_cast<type&>(value)) {}

    operator type() {
        ADAPTER_OF(district_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    operator const type() const {
        ADAPTER_OF(district_value)::CountRead(ColIndex + index_);
        return value_;
    }

    operator type&() {
        ADAPTER_OF(district_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    operator const type&() const {
        ADAPTER_OF(district_value)::CountRead(ColIndex + index_);
        return value_;
    }

    type operator =(const type& other) {
        ADAPTER_OF(district_value)::CountWrite(ColIndex + index_);
        return value_ = other;
    }

    type operator =(const accessor<ColIndex>& other) {
        ADAPTER_OF(district_value)::CountWrite(ColIndex + index_);
        return value_ = other.value_;
    }

    type operator *() {
        ADAPTER_OF(district_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    const type operator *() const {
        ADAPTER_OF(district_value)::CountRead(ColIndex + index_);
        return value_;
    }

    type* operator ->() {
        ADAPTER_OF(district_value)::CountWrite(ColIndex + index_);
        return &value_;
    }

    const type* operator ->() const {
        ADAPTER_OF(district_value)::CountRead(ColIndex + index_);
        return &value_;
    }

    size_t index_ = 0;
    type value_;
};

template <>
struct split_value<0, 1> {
    explicit split_value() = default;
    accessor<0> d_name;
};
template <>
struct split_value<1, 8> {
    explicit split_value() = default;
    accessor<1> d_street_1;
    accessor<2> d_street_2;
    accessor<3> d_city;
    accessor<4> d_state;
    accessor<5> d_zip;
    accessor<6> d_tax;
    accessor<7> d_ytd;
};
template <>
struct unified_value<1> {
    auto& d_name() {
        return split_0.d_name;
    }
    const auto& d_name() const {
        return split_0.d_name;
    }
    auto& d_street_1() {
        return split_1.d_street_1;
    }
    const auto& d_street_1() const {
        return split_1.d_street_1;
    }
    auto& d_street_2() {
        return split_1.d_street_2;
    }
    const auto& d_street_2() const {
        return split_1.d_street_2;
    }
    auto& d_city() {
        return split_1.d_city;
    }
    const auto& d_city() const {
        return split_1.d_city;
    }
    auto& d_state() {
        return split_1.d_state;
    }
    const auto& d_state() const {
        return split_1.d_state;
    }
    auto& d_zip() {
        return split_1.d_zip;
    }
    const auto& d_zip() const {
        return split_1.d_zip;
    }
    auto& d_tax() {
        return split_1.d_tax;
    }
    const auto& d_tax() const {
        return split_1.d_tax;
    }
    auto& d_ytd() {
        return split_1.d_ytd;
    }
    const auto& d_ytd() const {
        return split_1.d_ytd;
    }

    split_value<0, 1> split_0;
    split_value<1, 8> split_1;
};

template <>
struct split_value<0, 2> {
    explicit split_value() = default;
    accessor<0> d_name;
    accessor<1> d_street_1;
};
template <>
struct split_value<2, 8> {
    explicit split_value() = default;
    accessor<2> d_street_2;
    accessor<3> d_city;
    accessor<4> d_state;
    accessor<5> d_zip;
    accessor<6> d_tax;
    accessor<7> d_ytd;
};
template <>
struct unified_value<2> {
    auto& d_name() {
        return split_0.d_name;
    }
    const auto& d_name() const {
        return split_0.d_name;
    }
    auto& d_street_1() {
        return split_0.d_street_1;
    }
    const auto& d_street_1() const {
        return split_0.d_street_1;
    }
    auto& d_street_2() {
        return split_1.d_street_2;
    }
    const auto& d_street_2() const {
        return split_1.d_street_2;
    }
    auto& d_city() {
        return split_1.d_city;
    }
    const auto& d_city() const {
        return split_1.d_city;
    }
    auto& d_state() {
        return split_1.d_state;
    }
    const auto& d_state() const {
        return split_1.d_state;
    }
    auto& d_zip() {
        return split_1.d_zip;
    }
    const auto& d_zip() const {
        return split_1.d_zip;
    }
    auto& d_tax() {
        return split_1.d_tax;
    }
    const auto& d_tax() const {
        return split_1.d_tax;
    }
    auto& d_ytd() {
        return split_1.d_ytd;
    }
    const auto& d_ytd() const {
        return split_1.d_ytd;
    }

    split_value<0, 2> split_0;
    split_value<2, 8> split_1;
};

template <>
struct split_value<0, 3> {
    explicit split_value() = default;
    accessor<0> d_name;
    accessor<1> d_street_1;
    accessor<2> d_street_2;
};
template <>
struct split_value<3, 8> {
    explicit split_value() = default;
    accessor<3> d_city;
    accessor<4> d_state;
    accessor<5> d_zip;
    accessor<6> d_tax;
    accessor<7> d_ytd;
};
template <>
struct unified_value<3> {
    auto& d_name() {
        return split_0.d_name;
    }
    const auto& d_name() const {
        return split_0.d_name;
    }
    auto& d_street_1() {
        return split_0.d_street_1;
    }
    const auto& d_street_1() const {
        return split_0.d_street_1;
    }
    auto& d_street_2() {
        return split_0.d_street_2;
    }
    const auto& d_street_2() const {
        return split_0.d_street_2;
    }
    auto& d_city() {
        return split_1.d_city;
    }
    const auto& d_city() const {
        return split_1.d_city;
    }
    auto& d_state() {
        return split_1.d_state;
    }
    const auto& d_state() const {
        return split_1.d_state;
    }
    auto& d_zip() {
        return split_1.d_zip;
    }
    const auto& d_zip() const {
        return split_1.d_zip;
    }
    auto& d_tax() {
        return split_1.d_tax;
    }
    const auto& d_tax() const {
        return split_1.d_tax;
    }
    auto& d_ytd() {
        return split_1.d_ytd;
    }
    const auto& d_ytd() const {
        return split_1.d_ytd;
    }

    split_value<0, 3> split_0;
    split_value<3, 8> split_1;
};

template <>
struct split_value<0, 4> {
    explicit split_value() = default;
    accessor<0> d_name;
    accessor<1> d_street_1;
    accessor<2> d_street_2;
    accessor<3> d_city;
};
template <>
struct split_value<4, 8> {
    explicit split_value() = default;
    accessor<4> d_state;
    accessor<5> d_zip;
    accessor<6> d_tax;
    accessor<7> d_ytd;
};
template <>
struct unified_value<4> {
    auto& d_name() {
        return split_0.d_name;
    }
    const auto& d_name() const {
        return split_0.d_name;
    }
    auto& d_street_1() {
        return split_0.d_street_1;
    }
    const auto& d_street_1() const {
        return split_0.d_street_1;
    }
    auto& d_street_2() {
        return split_0.d_street_2;
    }
    const auto& d_street_2() const {
        return split_0.d_street_2;
    }
    auto& d_city() {
        return split_0.d_city;
    }
    const auto& d_city() const {
        return split_0.d_city;
    }
    auto& d_state() {
        return split_1.d_state;
    }
    const auto& d_state() const {
        return split_1.d_state;
    }
    auto& d_zip() {
        return split_1.d_zip;
    }
    const auto& d_zip() const {
        return split_1.d_zip;
    }
    auto& d_tax() {
        return split_1.d_tax;
    }
    const auto& d_tax() const {
        return split_1.d_tax;
    }
    auto& d_ytd() {
        return split_1.d_ytd;
    }
    const auto& d_ytd() const {
        return split_1.d_ytd;
    }

    split_value<0, 4> split_0;
    split_value<4, 8> split_1;
};

template <>
struct split_value<0, 5> {
    explicit split_value() = default;
    accessor<0> d_name;
    accessor<1> d_street_1;
    accessor<2> d_street_2;
    accessor<3> d_city;
    accessor<4> d_state;
};
template <>
struct split_value<5, 8> {
    explicit split_value() = default;
    accessor<5> d_zip;
    accessor<6> d_tax;
    accessor<7> d_ytd;
};
template <>
struct unified_value<5> {
    auto& d_name() {
        return split_0.d_name;
    }
    const auto& d_name() const {
        return split_0.d_name;
    }
    auto& d_street_1() {
        return split_0.d_street_1;
    }
    const auto& d_street_1() const {
        return split_0.d_street_1;
    }
    auto& d_street_2() {
        return split_0.d_street_2;
    }
    const auto& d_street_2() const {
        return split_0.d_street_2;
    }
    auto& d_city() {
        return split_0.d_city;
    }
    const auto& d_city() const {
        return split_0.d_city;
    }
    auto& d_state() {
        return split_0.d_state;
    }
    const auto& d_state() const {
        return split_0.d_state;
    }
    auto& d_zip() {
        return split_1.d_zip;
    }
    const auto& d_zip() const {
        return split_1.d_zip;
    }
    auto& d_tax() {
        return split_1.d_tax;
    }
    const auto& d_tax() const {
        return split_1.d_tax;
    }
    auto& d_ytd() {
        return split_1.d_ytd;
    }
    const auto& d_ytd() const {
        return split_1.d_ytd;
    }

    split_value<0, 5> split_0;
    split_value<5, 8> split_1;
};

template <>
struct split_value<0, 6> {
    explicit split_value() = default;
    accessor<0> d_name;
    accessor<1> d_street_1;
    accessor<2> d_street_2;
    accessor<3> d_city;
    accessor<4> d_state;
    accessor<5> d_zip;
};
template <>
struct split_value<6, 8> {
    explicit split_value() = default;
    accessor<6> d_tax;
    accessor<7> d_ytd;
};
template <>
struct unified_value<6> {
    auto& d_name() {
        return split_0.d_name;
    }
    const auto& d_name() const {
        return split_0.d_name;
    }
    auto& d_street_1() {
        return split_0.d_street_1;
    }
    const auto& d_street_1() const {
        return split_0.d_street_1;
    }
    auto& d_street_2() {
        return split_0.d_street_2;
    }
    const auto& d_street_2() const {
        return split_0.d_street_2;
    }
    auto& d_city() {
        return split_0.d_city;
    }
    const auto& d_city() const {
        return split_0.d_city;
    }
    auto& d_state() {
        return split_0.d_state;
    }
    const auto& d_state() const {
        return split_0.d_state;
    }
    auto& d_zip() {
        return split_0.d_zip;
    }
    const auto& d_zip() const {
        return split_0.d_zip;
    }
    auto& d_tax() {
        return split_1.d_tax;
    }
    const auto& d_tax() const {
        return split_1.d_tax;
    }
    auto& d_ytd() {
        return split_1.d_ytd;
    }
    const auto& d_ytd() const {
        return split_1.d_ytd;
    }

    split_value<0, 6> split_0;
    split_value<6, 8> split_1;
};

template <>
struct split_value<0, 7> {
    explicit split_value() = default;
    accessor<0> d_name;
    accessor<1> d_street_1;
    accessor<2> d_street_2;
    accessor<3> d_city;
    accessor<4> d_state;
    accessor<5> d_zip;
    accessor<6> d_tax;
};
template <>
struct split_value<7, 8> {
    explicit split_value() = default;
    accessor<7> d_ytd;
};
template <>
struct unified_value<7> {
    auto& d_name() {
        return split_0.d_name;
    }
    const auto& d_name() const {
        return split_0.d_name;
    }
    auto& d_street_1() {
        return split_0.d_street_1;
    }
    const auto& d_street_1() const {
        return split_0.d_street_1;
    }
    auto& d_street_2() {
        return split_0.d_street_2;
    }
    const auto& d_street_2() const {
        return split_0.d_street_2;
    }
    auto& d_city() {
        return split_0.d_city;
    }
    const auto& d_city() const {
        return split_0.d_city;
    }
    auto& d_state() {
        return split_0.d_state;
    }
    const auto& d_state() const {
        return split_0.d_state;
    }
    auto& d_zip() {
        return split_0.d_zip;
    }
    const auto& d_zip() const {
        return split_0.d_zip;
    }
    auto& d_tax() {
        return split_0.d_tax;
    }
    const auto& d_tax() const {
        return split_0.d_tax;
    }
    auto& d_ytd() {
        return split_1.d_ytd;
    }
    const auto& d_ytd() const {
        return split_1.d_ytd;
    }

    split_value<0, 7> split_0;
    split_value<7, 8> split_1;
};

template <>
struct split_value<0, 8> {
    explicit split_value() = default;
    accessor<0> d_name;
    accessor<1> d_street_1;
    accessor<2> d_street_2;
    accessor<3> d_city;
    accessor<4> d_state;
    accessor<5> d_zip;
    accessor<6> d_tax;
    accessor<7> d_ytd;
};
template <>
struct unified_value<8> {
    auto& d_name() {
        return split_0.d_name;
    }
    const auto& d_name() const {
        return split_0.d_name;
    }
    auto& d_street_1() {
        return split_0.d_street_1;
    }
    const auto& d_street_1() const {
        return split_0.d_street_1;
    }
    auto& d_street_2() {
        return split_0.d_street_2;
    }
    const auto& d_street_2() const {
        return split_0.d_street_2;
    }
    auto& d_city() {
        return split_0.d_city;
    }
    const auto& d_city() const {
        return split_0.d_city;
    }
    auto& d_state() {
        return split_0.d_state;
    }
    const auto& d_state() const {
        return split_0.d_state;
    }
    auto& d_zip() {
        return split_0.d_zip;
    }
    const auto& d_zip() const {
        return split_0.d_zip;
    }
    auto& d_tax() {
        return split_0.d_tax;
    }
    const auto& d_tax() const {
        return split_0.d_tax;
    }
    auto& d_ytd() {
        return split_0.d_ytd;
    }
    const auto& d_ytd() const {
        return split_0.d_ytd;
    }

    split_value<0, 8> split_0;
};

struct district_value {
    explicit district_value() = default;

    using NamedColumn = district_value_datatypes::NamedColumn;

    auto& d_name() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.d_name();
            }, value);
    }
    const auto& d_name() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.d_name();
            }, value);
    }
    auto& d_street_1() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.d_street_1();
            }, value);
    }
    const auto& d_street_1() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.d_street_1();
            }, value);
    }
    auto& d_street_2() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.d_street_2();
            }, value);
    }
    const auto& d_street_2() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.d_street_2();
            }, value);
    }
    auto& d_city() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.d_city();
            }, value);
    }
    const auto& d_city() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.d_city();
            }, value);
    }
    auto& d_state() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.d_state();
            }, value);
    }
    const auto& d_state() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.d_state();
            }, value);
    }
    auto& d_zip() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.d_zip();
            }, value);
    }
    const auto& d_zip() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.d_zip();
            }, value);
    }
    auto& d_tax() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.d_tax();
            }, value);
    }
    const auto& d_tax() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.d_tax();
            }, value);
    }
    auto& d_ytd() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.d_ytd();
            }, value);
    }
    const auto& d_ytd() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.d_ytd();
            }, value);
    }

    std::variant<
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
};  // namespace district_value_datatypes

using district_value = district_value_datatypes::district_value;
using ADAPTER_OF(district_value) = ADAPTER_OF(district_value_datatypes::district_value);

namespace customer_idx_value_datatypes {

enum class NamedColumn : int {
    c_ids = 0,
    COLCOUNT = 1
};

template <size_t ColIndex>
struct accessor;

template <size_t StartIndex, size_t EndIndex>
struct split_value;

template <size_t SplitIndex>
struct unified_value;

struct customer_idx_value;

DEFINE_ADAPTER(customer_idx_value, 1);

template <size_t ColIndex>
struct accessor_info;

template <>
struct accessor_info<0> {
    using type = std::list<uint64_t>;
};

template <size_t ColIndex>
struct accessor {
    using type = typename accessor_info<ColIndex>::type;

    accessor() = default;
    accessor(type& value) : value_(value) {}
    accessor(const type& value) : value_(const_cast<type&>(value)) {}

    operator type() {
        ADAPTER_OF(customer_idx_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    operator const type() const {
        ADAPTER_OF(customer_idx_value)::CountRead(ColIndex + index_);
        return value_;
    }

    operator type&() {
        ADAPTER_OF(customer_idx_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    operator const type&() const {
        ADAPTER_OF(customer_idx_value)::CountRead(ColIndex + index_);
        return value_;
    }

    type operator =(const type& other) {
        ADAPTER_OF(customer_idx_value)::CountWrite(ColIndex + index_);
        return value_ = other;
    }

    type operator =(const accessor<ColIndex>& other) {
        ADAPTER_OF(customer_idx_value)::CountWrite(ColIndex + index_);
        return value_ = other.value_;
    }

    type operator *() {
        ADAPTER_OF(customer_idx_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    const type operator *() const {
        ADAPTER_OF(customer_idx_value)::CountRead(ColIndex + index_);
        return value_;
    }

    type* operator ->() {
        ADAPTER_OF(customer_idx_value)::CountWrite(ColIndex + index_);
        return &value_;
    }

    const type* operator ->() const {
        ADAPTER_OF(customer_idx_value)::CountRead(ColIndex + index_);
        return &value_;
    }

    size_t index_ = 0;
    type value_;
};

template <>
struct split_value<0, 1> {
    explicit split_value() = default;
    accessor<0> c_ids;
};
template <>
struct unified_value<1> {
    auto& c_ids() {
        return split_0.c_ids;
    }
    const auto& c_ids() const {
        return split_0.c_ids;
    }

    split_value<0, 1> split_0;
};

struct customer_idx_value {
    explicit customer_idx_value() = default;

    using NamedColumn = customer_idx_value_datatypes::NamedColumn;

    auto& c_ids() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.c_ids();
            }, value);
    }
    const auto& c_ids() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.c_ids();
            }, value);
    }

    std::variant<
        unified_value<1>
        > value;
};
};  // namespace customer_idx_value_datatypes

using customer_idx_value = customer_idx_value_datatypes::customer_idx_value;
using ADAPTER_OF(customer_idx_value) = ADAPTER_OF(customer_idx_value_datatypes::customer_idx_value);

namespace customer_value_datatypes {

enum class NamedColumn : int {
    c_first = 0,
    c_middle = 1,
    c_last = 2,
    c_street_1 = 3,
    c_street_2 = 4,
    c_city = 5,
    c_state = 6,
    c_zip = 7,
    c_phone = 8,
    c_since = 9,
    c_credit = 10,
    c_credit_lim = 11,
    c_discount = 12,
    c_balance = 13,
    c_ytd_payment = 14,
    c_payment_cnt = 15,
    c_delivery_cnt = 16,
    c_data = 17,
    COLCOUNT = 18
};

template <size_t ColIndex>
struct accessor;

template <size_t StartIndex, size_t EndIndex>
struct split_value;

template <size_t SplitIndex>
struct unified_value;

struct customer_value;

DEFINE_ADAPTER(customer_value, 18);

template <size_t ColIndex>
struct accessor_info;

template <>
struct accessor_info<0> {
    using type = var_string<16>;
};

template <>
struct accessor_info<1> {
    using type = fix_string<2>;
};

template <>
struct accessor_info<2> {
    using type = var_string<16>;
};

template <>
struct accessor_info<3> {
    using type = var_string<20>;
};

template <>
struct accessor_info<4> {
    using type = var_string<20>;
};

template <>
struct accessor_info<5> {
    using type = var_string<20>;
};

template <>
struct accessor_info<6> {
    using type = fix_string<2>;
};

template <>
struct accessor_info<7> {
    using type = fix_string<9>;
};

template <>
struct accessor_info<8> {
    using type = fix_string<16>;
};

template <>
struct accessor_info<9> {
    using type = uint32_t;
};

template <>
struct accessor_info<10> {
    using type = fix_string<2>;
};

template <>
struct accessor_info<11> {
    using type = int64_t;
};

template <>
struct accessor_info<12> {
    using type = int64_t;
};

template <>
struct accessor_info<13> {
    using type = int64_t;
};

template <>
struct accessor_info<14> {
    using type = int64_t;
};

template <>
struct accessor_info<15> {
    using type = uint16_t;
};

template <>
struct accessor_info<16> {
    using type = uint16_t;
};

template <>
struct accessor_info<17> {
    using type = fix_string<500>;
};

template <size_t ColIndex>
struct accessor {
    using type = typename accessor_info<ColIndex>::type;

    accessor() = default;
    accessor(type& value) : value_(value) {}
    accessor(const type& value) : value_(const_cast<type&>(value)) {}

    operator type() {
        ADAPTER_OF(customer_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    operator const type() const {
        ADAPTER_OF(customer_value)::CountRead(ColIndex + index_);
        return value_;
    }

    operator type&() {
        ADAPTER_OF(customer_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    operator const type&() const {
        ADAPTER_OF(customer_value)::CountRead(ColIndex + index_);
        return value_;
    }

    type operator =(const type& other) {
        ADAPTER_OF(customer_value)::CountWrite(ColIndex + index_);
        return value_ = other;
    }

    type operator =(const accessor<ColIndex>& other) {
        ADAPTER_OF(customer_value)::CountWrite(ColIndex + index_);
        return value_ = other.value_;
    }

    type operator *() {
        ADAPTER_OF(customer_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    const type operator *() const {
        ADAPTER_OF(customer_value)::CountRead(ColIndex + index_);
        return value_;
    }

    type* operator ->() {
        ADAPTER_OF(customer_value)::CountWrite(ColIndex + index_);
        return &value_;
    }

    const type* operator ->() const {
        ADAPTER_OF(customer_value)::CountRead(ColIndex + index_);
        return &value_;
    }

    size_t index_ = 0;
    type value_;
};

template <>
struct split_value<0, 1> {
    explicit split_value() = default;
    accessor<0> c_first;
};
template <>
struct split_value<1, 18> {
    explicit split_value() = default;
    accessor<1> c_middle;
    accessor<2> c_last;
    accessor<3> c_street_1;
    accessor<4> c_street_2;
    accessor<5> c_city;
    accessor<6> c_state;
    accessor<7> c_zip;
    accessor<8> c_phone;
    accessor<9> c_since;
    accessor<10> c_credit;
    accessor<11> c_credit_lim;
    accessor<12> c_discount;
    accessor<13> c_balance;
    accessor<14> c_ytd_payment;
    accessor<15> c_payment_cnt;
    accessor<16> c_delivery_cnt;
    accessor<17> c_data;
};
template <>
struct unified_value<1> {
    auto& c_first() {
        return split_0.c_first;
    }
    const auto& c_first() const {
        return split_0.c_first;
    }
    auto& c_middle() {
        return split_1.c_middle;
    }
    const auto& c_middle() const {
        return split_1.c_middle;
    }
    auto& c_last() {
        return split_1.c_last;
    }
    const auto& c_last() const {
        return split_1.c_last;
    }
    auto& c_street_1() {
        return split_1.c_street_1;
    }
    const auto& c_street_1() const {
        return split_1.c_street_1;
    }
    auto& c_street_2() {
        return split_1.c_street_2;
    }
    const auto& c_street_2() const {
        return split_1.c_street_2;
    }
    auto& c_city() {
        return split_1.c_city;
    }
    const auto& c_city() const {
        return split_1.c_city;
    }
    auto& c_state() {
        return split_1.c_state;
    }
    const auto& c_state() const {
        return split_1.c_state;
    }
    auto& c_zip() {
        return split_1.c_zip;
    }
    const auto& c_zip() const {
        return split_1.c_zip;
    }
    auto& c_phone() {
        return split_1.c_phone;
    }
    const auto& c_phone() const {
        return split_1.c_phone;
    }
    auto& c_since() {
        return split_1.c_since;
    }
    const auto& c_since() const {
        return split_1.c_since;
    }
    auto& c_credit() {
        return split_1.c_credit;
    }
    const auto& c_credit() const {
        return split_1.c_credit;
    }
    auto& c_credit_lim() {
        return split_1.c_credit_lim;
    }
    const auto& c_credit_lim() const {
        return split_1.c_credit_lim;
    }
    auto& c_discount() {
        return split_1.c_discount;
    }
    const auto& c_discount() const {
        return split_1.c_discount;
    }
    auto& c_balance() {
        return split_1.c_balance;
    }
    const auto& c_balance() const {
        return split_1.c_balance;
    }
    auto& c_ytd_payment() {
        return split_1.c_ytd_payment;
    }
    const auto& c_ytd_payment() const {
        return split_1.c_ytd_payment;
    }
    auto& c_payment_cnt() {
        return split_1.c_payment_cnt;
    }
    const auto& c_payment_cnt() const {
        return split_1.c_payment_cnt;
    }
    auto& c_delivery_cnt() {
        return split_1.c_delivery_cnt;
    }
    const auto& c_delivery_cnt() const {
        return split_1.c_delivery_cnt;
    }
    auto& c_data() {
        return split_1.c_data;
    }
    const auto& c_data() const {
        return split_1.c_data;
    }

    split_value<0, 1> split_0;
    split_value<1, 18> split_1;
};

template <>
struct split_value<0, 2> {
    explicit split_value() = default;
    accessor<0> c_first;
    accessor<1> c_middle;
};
template <>
struct split_value<2, 18> {
    explicit split_value() = default;
    accessor<2> c_last;
    accessor<3> c_street_1;
    accessor<4> c_street_2;
    accessor<5> c_city;
    accessor<6> c_state;
    accessor<7> c_zip;
    accessor<8> c_phone;
    accessor<9> c_since;
    accessor<10> c_credit;
    accessor<11> c_credit_lim;
    accessor<12> c_discount;
    accessor<13> c_balance;
    accessor<14> c_ytd_payment;
    accessor<15> c_payment_cnt;
    accessor<16> c_delivery_cnt;
    accessor<17> c_data;
};
template <>
struct unified_value<2> {
    auto& c_first() {
        return split_0.c_first;
    }
    const auto& c_first() const {
        return split_0.c_first;
    }
    auto& c_middle() {
        return split_0.c_middle;
    }
    const auto& c_middle() const {
        return split_0.c_middle;
    }
    auto& c_last() {
        return split_1.c_last;
    }
    const auto& c_last() const {
        return split_1.c_last;
    }
    auto& c_street_1() {
        return split_1.c_street_1;
    }
    const auto& c_street_1() const {
        return split_1.c_street_1;
    }
    auto& c_street_2() {
        return split_1.c_street_2;
    }
    const auto& c_street_2() const {
        return split_1.c_street_2;
    }
    auto& c_city() {
        return split_1.c_city;
    }
    const auto& c_city() const {
        return split_1.c_city;
    }
    auto& c_state() {
        return split_1.c_state;
    }
    const auto& c_state() const {
        return split_1.c_state;
    }
    auto& c_zip() {
        return split_1.c_zip;
    }
    const auto& c_zip() const {
        return split_1.c_zip;
    }
    auto& c_phone() {
        return split_1.c_phone;
    }
    const auto& c_phone() const {
        return split_1.c_phone;
    }
    auto& c_since() {
        return split_1.c_since;
    }
    const auto& c_since() const {
        return split_1.c_since;
    }
    auto& c_credit() {
        return split_1.c_credit;
    }
    const auto& c_credit() const {
        return split_1.c_credit;
    }
    auto& c_credit_lim() {
        return split_1.c_credit_lim;
    }
    const auto& c_credit_lim() const {
        return split_1.c_credit_lim;
    }
    auto& c_discount() {
        return split_1.c_discount;
    }
    const auto& c_discount() const {
        return split_1.c_discount;
    }
    auto& c_balance() {
        return split_1.c_balance;
    }
    const auto& c_balance() const {
        return split_1.c_balance;
    }
    auto& c_ytd_payment() {
        return split_1.c_ytd_payment;
    }
    const auto& c_ytd_payment() const {
        return split_1.c_ytd_payment;
    }
    auto& c_payment_cnt() {
        return split_1.c_payment_cnt;
    }
    const auto& c_payment_cnt() const {
        return split_1.c_payment_cnt;
    }
    auto& c_delivery_cnt() {
        return split_1.c_delivery_cnt;
    }
    const auto& c_delivery_cnt() const {
        return split_1.c_delivery_cnt;
    }
    auto& c_data() {
        return split_1.c_data;
    }
    const auto& c_data() const {
        return split_1.c_data;
    }

    split_value<0, 2> split_0;
    split_value<2, 18> split_1;
};

template <>
struct split_value<0, 3> {
    explicit split_value() = default;
    accessor<0> c_first;
    accessor<1> c_middle;
    accessor<2> c_last;
};
template <>
struct split_value<3, 18> {
    explicit split_value() = default;
    accessor<3> c_street_1;
    accessor<4> c_street_2;
    accessor<5> c_city;
    accessor<6> c_state;
    accessor<7> c_zip;
    accessor<8> c_phone;
    accessor<9> c_since;
    accessor<10> c_credit;
    accessor<11> c_credit_lim;
    accessor<12> c_discount;
    accessor<13> c_balance;
    accessor<14> c_ytd_payment;
    accessor<15> c_payment_cnt;
    accessor<16> c_delivery_cnt;
    accessor<17> c_data;
};
template <>
struct unified_value<3> {
    auto& c_first() {
        return split_0.c_first;
    }
    const auto& c_first() const {
        return split_0.c_first;
    }
    auto& c_middle() {
        return split_0.c_middle;
    }
    const auto& c_middle() const {
        return split_0.c_middle;
    }
    auto& c_last() {
        return split_0.c_last;
    }
    const auto& c_last() const {
        return split_0.c_last;
    }
    auto& c_street_1() {
        return split_1.c_street_1;
    }
    const auto& c_street_1() const {
        return split_1.c_street_1;
    }
    auto& c_street_2() {
        return split_1.c_street_2;
    }
    const auto& c_street_2() const {
        return split_1.c_street_2;
    }
    auto& c_city() {
        return split_1.c_city;
    }
    const auto& c_city() const {
        return split_1.c_city;
    }
    auto& c_state() {
        return split_1.c_state;
    }
    const auto& c_state() const {
        return split_1.c_state;
    }
    auto& c_zip() {
        return split_1.c_zip;
    }
    const auto& c_zip() const {
        return split_1.c_zip;
    }
    auto& c_phone() {
        return split_1.c_phone;
    }
    const auto& c_phone() const {
        return split_1.c_phone;
    }
    auto& c_since() {
        return split_1.c_since;
    }
    const auto& c_since() const {
        return split_1.c_since;
    }
    auto& c_credit() {
        return split_1.c_credit;
    }
    const auto& c_credit() const {
        return split_1.c_credit;
    }
    auto& c_credit_lim() {
        return split_1.c_credit_lim;
    }
    const auto& c_credit_lim() const {
        return split_1.c_credit_lim;
    }
    auto& c_discount() {
        return split_1.c_discount;
    }
    const auto& c_discount() const {
        return split_1.c_discount;
    }
    auto& c_balance() {
        return split_1.c_balance;
    }
    const auto& c_balance() const {
        return split_1.c_balance;
    }
    auto& c_ytd_payment() {
        return split_1.c_ytd_payment;
    }
    const auto& c_ytd_payment() const {
        return split_1.c_ytd_payment;
    }
    auto& c_payment_cnt() {
        return split_1.c_payment_cnt;
    }
    const auto& c_payment_cnt() const {
        return split_1.c_payment_cnt;
    }
    auto& c_delivery_cnt() {
        return split_1.c_delivery_cnt;
    }
    const auto& c_delivery_cnt() const {
        return split_1.c_delivery_cnt;
    }
    auto& c_data() {
        return split_1.c_data;
    }
    const auto& c_data() const {
        return split_1.c_data;
    }

    split_value<0, 3> split_0;
    split_value<3, 18> split_1;
};

template <>
struct split_value<0, 4> {
    explicit split_value() = default;
    accessor<0> c_first;
    accessor<1> c_middle;
    accessor<2> c_last;
    accessor<3> c_street_1;
};
template <>
struct split_value<4, 18> {
    explicit split_value() = default;
    accessor<4> c_street_2;
    accessor<5> c_city;
    accessor<6> c_state;
    accessor<7> c_zip;
    accessor<8> c_phone;
    accessor<9> c_since;
    accessor<10> c_credit;
    accessor<11> c_credit_lim;
    accessor<12> c_discount;
    accessor<13> c_balance;
    accessor<14> c_ytd_payment;
    accessor<15> c_payment_cnt;
    accessor<16> c_delivery_cnt;
    accessor<17> c_data;
};
template <>
struct unified_value<4> {
    auto& c_first() {
        return split_0.c_first;
    }
    const auto& c_first() const {
        return split_0.c_first;
    }
    auto& c_middle() {
        return split_0.c_middle;
    }
    const auto& c_middle() const {
        return split_0.c_middle;
    }
    auto& c_last() {
        return split_0.c_last;
    }
    const auto& c_last() const {
        return split_0.c_last;
    }
    auto& c_street_1() {
        return split_0.c_street_1;
    }
    const auto& c_street_1() const {
        return split_0.c_street_1;
    }
    auto& c_street_2() {
        return split_1.c_street_2;
    }
    const auto& c_street_2() const {
        return split_1.c_street_2;
    }
    auto& c_city() {
        return split_1.c_city;
    }
    const auto& c_city() const {
        return split_1.c_city;
    }
    auto& c_state() {
        return split_1.c_state;
    }
    const auto& c_state() const {
        return split_1.c_state;
    }
    auto& c_zip() {
        return split_1.c_zip;
    }
    const auto& c_zip() const {
        return split_1.c_zip;
    }
    auto& c_phone() {
        return split_1.c_phone;
    }
    const auto& c_phone() const {
        return split_1.c_phone;
    }
    auto& c_since() {
        return split_1.c_since;
    }
    const auto& c_since() const {
        return split_1.c_since;
    }
    auto& c_credit() {
        return split_1.c_credit;
    }
    const auto& c_credit() const {
        return split_1.c_credit;
    }
    auto& c_credit_lim() {
        return split_1.c_credit_lim;
    }
    const auto& c_credit_lim() const {
        return split_1.c_credit_lim;
    }
    auto& c_discount() {
        return split_1.c_discount;
    }
    const auto& c_discount() const {
        return split_1.c_discount;
    }
    auto& c_balance() {
        return split_1.c_balance;
    }
    const auto& c_balance() const {
        return split_1.c_balance;
    }
    auto& c_ytd_payment() {
        return split_1.c_ytd_payment;
    }
    const auto& c_ytd_payment() const {
        return split_1.c_ytd_payment;
    }
    auto& c_payment_cnt() {
        return split_1.c_payment_cnt;
    }
    const auto& c_payment_cnt() const {
        return split_1.c_payment_cnt;
    }
    auto& c_delivery_cnt() {
        return split_1.c_delivery_cnt;
    }
    const auto& c_delivery_cnt() const {
        return split_1.c_delivery_cnt;
    }
    auto& c_data() {
        return split_1.c_data;
    }
    const auto& c_data() const {
        return split_1.c_data;
    }

    split_value<0, 4> split_0;
    split_value<4, 18> split_1;
};

template <>
struct split_value<0, 5> {
    explicit split_value() = default;
    accessor<0> c_first;
    accessor<1> c_middle;
    accessor<2> c_last;
    accessor<3> c_street_1;
    accessor<4> c_street_2;
};
template <>
struct split_value<5, 18> {
    explicit split_value() = default;
    accessor<5> c_city;
    accessor<6> c_state;
    accessor<7> c_zip;
    accessor<8> c_phone;
    accessor<9> c_since;
    accessor<10> c_credit;
    accessor<11> c_credit_lim;
    accessor<12> c_discount;
    accessor<13> c_balance;
    accessor<14> c_ytd_payment;
    accessor<15> c_payment_cnt;
    accessor<16> c_delivery_cnt;
    accessor<17> c_data;
};
template <>
struct unified_value<5> {
    auto& c_first() {
        return split_0.c_first;
    }
    const auto& c_first() const {
        return split_0.c_first;
    }
    auto& c_middle() {
        return split_0.c_middle;
    }
    const auto& c_middle() const {
        return split_0.c_middle;
    }
    auto& c_last() {
        return split_0.c_last;
    }
    const auto& c_last() const {
        return split_0.c_last;
    }
    auto& c_street_1() {
        return split_0.c_street_1;
    }
    const auto& c_street_1() const {
        return split_0.c_street_1;
    }
    auto& c_street_2() {
        return split_0.c_street_2;
    }
    const auto& c_street_2() const {
        return split_0.c_street_2;
    }
    auto& c_city() {
        return split_1.c_city;
    }
    const auto& c_city() const {
        return split_1.c_city;
    }
    auto& c_state() {
        return split_1.c_state;
    }
    const auto& c_state() const {
        return split_1.c_state;
    }
    auto& c_zip() {
        return split_1.c_zip;
    }
    const auto& c_zip() const {
        return split_1.c_zip;
    }
    auto& c_phone() {
        return split_1.c_phone;
    }
    const auto& c_phone() const {
        return split_1.c_phone;
    }
    auto& c_since() {
        return split_1.c_since;
    }
    const auto& c_since() const {
        return split_1.c_since;
    }
    auto& c_credit() {
        return split_1.c_credit;
    }
    const auto& c_credit() const {
        return split_1.c_credit;
    }
    auto& c_credit_lim() {
        return split_1.c_credit_lim;
    }
    const auto& c_credit_lim() const {
        return split_1.c_credit_lim;
    }
    auto& c_discount() {
        return split_1.c_discount;
    }
    const auto& c_discount() const {
        return split_1.c_discount;
    }
    auto& c_balance() {
        return split_1.c_balance;
    }
    const auto& c_balance() const {
        return split_1.c_balance;
    }
    auto& c_ytd_payment() {
        return split_1.c_ytd_payment;
    }
    const auto& c_ytd_payment() const {
        return split_1.c_ytd_payment;
    }
    auto& c_payment_cnt() {
        return split_1.c_payment_cnt;
    }
    const auto& c_payment_cnt() const {
        return split_1.c_payment_cnt;
    }
    auto& c_delivery_cnt() {
        return split_1.c_delivery_cnt;
    }
    const auto& c_delivery_cnt() const {
        return split_1.c_delivery_cnt;
    }
    auto& c_data() {
        return split_1.c_data;
    }
    const auto& c_data() const {
        return split_1.c_data;
    }

    split_value<0, 5> split_0;
    split_value<5, 18> split_1;
};

template <>
struct split_value<0, 6> {
    explicit split_value() = default;
    accessor<0> c_first;
    accessor<1> c_middle;
    accessor<2> c_last;
    accessor<3> c_street_1;
    accessor<4> c_street_2;
    accessor<5> c_city;
};
template <>
struct split_value<6, 18> {
    explicit split_value() = default;
    accessor<6> c_state;
    accessor<7> c_zip;
    accessor<8> c_phone;
    accessor<9> c_since;
    accessor<10> c_credit;
    accessor<11> c_credit_lim;
    accessor<12> c_discount;
    accessor<13> c_balance;
    accessor<14> c_ytd_payment;
    accessor<15> c_payment_cnt;
    accessor<16> c_delivery_cnt;
    accessor<17> c_data;
};
template <>
struct unified_value<6> {
    auto& c_first() {
        return split_0.c_first;
    }
    const auto& c_first() const {
        return split_0.c_first;
    }
    auto& c_middle() {
        return split_0.c_middle;
    }
    const auto& c_middle() const {
        return split_0.c_middle;
    }
    auto& c_last() {
        return split_0.c_last;
    }
    const auto& c_last() const {
        return split_0.c_last;
    }
    auto& c_street_1() {
        return split_0.c_street_1;
    }
    const auto& c_street_1() const {
        return split_0.c_street_1;
    }
    auto& c_street_2() {
        return split_0.c_street_2;
    }
    const auto& c_street_2() const {
        return split_0.c_street_2;
    }
    auto& c_city() {
        return split_0.c_city;
    }
    const auto& c_city() const {
        return split_0.c_city;
    }
    auto& c_state() {
        return split_1.c_state;
    }
    const auto& c_state() const {
        return split_1.c_state;
    }
    auto& c_zip() {
        return split_1.c_zip;
    }
    const auto& c_zip() const {
        return split_1.c_zip;
    }
    auto& c_phone() {
        return split_1.c_phone;
    }
    const auto& c_phone() const {
        return split_1.c_phone;
    }
    auto& c_since() {
        return split_1.c_since;
    }
    const auto& c_since() const {
        return split_1.c_since;
    }
    auto& c_credit() {
        return split_1.c_credit;
    }
    const auto& c_credit() const {
        return split_1.c_credit;
    }
    auto& c_credit_lim() {
        return split_1.c_credit_lim;
    }
    const auto& c_credit_lim() const {
        return split_1.c_credit_lim;
    }
    auto& c_discount() {
        return split_1.c_discount;
    }
    const auto& c_discount() const {
        return split_1.c_discount;
    }
    auto& c_balance() {
        return split_1.c_balance;
    }
    const auto& c_balance() const {
        return split_1.c_balance;
    }
    auto& c_ytd_payment() {
        return split_1.c_ytd_payment;
    }
    const auto& c_ytd_payment() const {
        return split_1.c_ytd_payment;
    }
    auto& c_payment_cnt() {
        return split_1.c_payment_cnt;
    }
    const auto& c_payment_cnt() const {
        return split_1.c_payment_cnt;
    }
    auto& c_delivery_cnt() {
        return split_1.c_delivery_cnt;
    }
    const auto& c_delivery_cnt() const {
        return split_1.c_delivery_cnt;
    }
    auto& c_data() {
        return split_1.c_data;
    }
    const auto& c_data() const {
        return split_1.c_data;
    }

    split_value<0, 6> split_0;
    split_value<6, 18> split_1;
};

template <>
struct split_value<0, 7> {
    explicit split_value() = default;
    accessor<0> c_first;
    accessor<1> c_middle;
    accessor<2> c_last;
    accessor<3> c_street_1;
    accessor<4> c_street_2;
    accessor<5> c_city;
    accessor<6> c_state;
};
template <>
struct split_value<7, 18> {
    explicit split_value() = default;
    accessor<7> c_zip;
    accessor<8> c_phone;
    accessor<9> c_since;
    accessor<10> c_credit;
    accessor<11> c_credit_lim;
    accessor<12> c_discount;
    accessor<13> c_balance;
    accessor<14> c_ytd_payment;
    accessor<15> c_payment_cnt;
    accessor<16> c_delivery_cnt;
    accessor<17> c_data;
};
template <>
struct unified_value<7> {
    auto& c_first() {
        return split_0.c_first;
    }
    const auto& c_first() const {
        return split_0.c_first;
    }
    auto& c_middle() {
        return split_0.c_middle;
    }
    const auto& c_middle() const {
        return split_0.c_middle;
    }
    auto& c_last() {
        return split_0.c_last;
    }
    const auto& c_last() const {
        return split_0.c_last;
    }
    auto& c_street_1() {
        return split_0.c_street_1;
    }
    const auto& c_street_1() const {
        return split_0.c_street_1;
    }
    auto& c_street_2() {
        return split_0.c_street_2;
    }
    const auto& c_street_2() const {
        return split_0.c_street_2;
    }
    auto& c_city() {
        return split_0.c_city;
    }
    const auto& c_city() const {
        return split_0.c_city;
    }
    auto& c_state() {
        return split_0.c_state;
    }
    const auto& c_state() const {
        return split_0.c_state;
    }
    auto& c_zip() {
        return split_1.c_zip;
    }
    const auto& c_zip() const {
        return split_1.c_zip;
    }
    auto& c_phone() {
        return split_1.c_phone;
    }
    const auto& c_phone() const {
        return split_1.c_phone;
    }
    auto& c_since() {
        return split_1.c_since;
    }
    const auto& c_since() const {
        return split_1.c_since;
    }
    auto& c_credit() {
        return split_1.c_credit;
    }
    const auto& c_credit() const {
        return split_1.c_credit;
    }
    auto& c_credit_lim() {
        return split_1.c_credit_lim;
    }
    const auto& c_credit_lim() const {
        return split_1.c_credit_lim;
    }
    auto& c_discount() {
        return split_1.c_discount;
    }
    const auto& c_discount() const {
        return split_1.c_discount;
    }
    auto& c_balance() {
        return split_1.c_balance;
    }
    const auto& c_balance() const {
        return split_1.c_balance;
    }
    auto& c_ytd_payment() {
        return split_1.c_ytd_payment;
    }
    const auto& c_ytd_payment() const {
        return split_1.c_ytd_payment;
    }
    auto& c_payment_cnt() {
        return split_1.c_payment_cnt;
    }
    const auto& c_payment_cnt() const {
        return split_1.c_payment_cnt;
    }
    auto& c_delivery_cnt() {
        return split_1.c_delivery_cnt;
    }
    const auto& c_delivery_cnt() const {
        return split_1.c_delivery_cnt;
    }
    auto& c_data() {
        return split_1.c_data;
    }
    const auto& c_data() const {
        return split_1.c_data;
    }

    split_value<0, 7> split_0;
    split_value<7, 18> split_1;
};

template <>
struct split_value<0, 8> {
    explicit split_value() = default;
    accessor<0> c_first;
    accessor<1> c_middle;
    accessor<2> c_last;
    accessor<3> c_street_1;
    accessor<4> c_street_2;
    accessor<5> c_city;
    accessor<6> c_state;
    accessor<7> c_zip;
};
template <>
struct split_value<8, 18> {
    explicit split_value() = default;
    accessor<8> c_phone;
    accessor<9> c_since;
    accessor<10> c_credit;
    accessor<11> c_credit_lim;
    accessor<12> c_discount;
    accessor<13> c_balance;
    accessor<14> c_ytd_payment;
    accessor<15> c_payment_cnt;
    accessor<16> c_delivery_cnt;
    accessor<17> c_data;
};
template <>
struct unified_value<8> {
    auto& c_first() {
        return split_0.c_first;
    }
    const auto& c_first() const {
        return split_0.c_first;
    }
    auto& c_middle() {
        return split_0.c_middle;
    }
    const auto& c_middle() const {
        return split_0.c_middle;
    }
    auto& c_last() {
        return split_0.c_last;
    }
    const auto& c_last() const {
        return split_0.c_last;
    }
    auto& c_street_1() {
        return split_0.c_street_1;
    }
    const auto& c_street_1() const {
        return split_0.c_street_1;
    }
    auto& c_street_2() {
        return split_0.c_street_2;
    }
    const auto& c_street_2() const {
        return split_0.c_street_2;
    }
    auto& c_city() {
        return split_0.c_city;
    }
    const auto& c_city() const {
        return split_0.c_city;
    }
    auto& c_state() {
        return split_0.c_state;
    }
    const auto& c_state() const {
        return split_0.c_state;
    }
    auto& c_zip() {
        return split_0.c_zip;
    }
    const auto& c_zip() const {
        return split_0.c_zip;
    }
    auto& c_phone() {
        return split_1.c_phone;
    }
    const auto& c_phone() const {
        return split_1.c_phone;
    }
    auto& c_since() {
        return split_1.c_since;
    }
    const auto& c_since() const {
        return split_1.c_since;
    }
    auto& c_credit() {
        return split_1.c_credit;
    }
    const auto& c_credit() const {
        return split_1.c_credit;
    }
    auto& c_credit_lim() {
        return split_1.c_credit_lim;
    }
    const auto& c_credit_lim() const {
        return split_1.c_credit_lim;
    }
    auto& c_discount() {
        return split_1.c_discount;
    }
    const auto& c_discount() const {
        return split_1.c_discount;
    }
    auto& c_balance() {
        return split_1.c_balance;
    }
    const auto& c_balance() const {
        return split_1.c_balance;
    }
    auto& c_ytd_payment() {
        return split_1.c_ytd_payment;
    }
    const auto& c_ytd_payment() const {
        return split_1.c_ytd_payment;
    }
    auto& c_payment_cnt() {
        return split_1.c_payment_cnt;
    }
    const auto& c_payment_cnt() const {
        return split_1.c_payment_cnt;
    }
    auto& c_delivery_cnt() {
        return split_1.c_delivery_cnt;
    }
    const auto& c_delivery_cnt() const {
        return split_1.c_delivery_cnt;
    }
    auto& c_data() {
        return split_1.c_data;
    }
    const auto& c_data() const {
        return split_1.c_data;
    }

    split_value<0, 8> split_0;
    split_value<8, 18> split_1;
};

template <>
struct split_value<0, 9> {
    explicit split_value() = default;
    accessor<0> c_first;
    accessor<1> c_middle;
    accessor<2> c_last;
    accessor<3> c_street_1;
    accessor<4> c_street_2;
    accessor<5> c_city;
    accessor<6> c_state;
    accessor<7> c_zip;
    accessor<8> c_phone;
};
template <>
struct split_value<9, 18> {
    explicit split_value() = default;
    accessor<9> c_since;
    accessor<10> c_credit;
    accessor<11> c_credit_lim;
    accessor<12> c_discount;
    accessor<13> c_balance;
    accessor<14> c_ytd_payment;
    accessor<15> c_payment_cnt;
    accessor<16> c_delivery_cnt;
    accessor<17> c_data;
};
template <>
struct unified_value<9> {
    auto& c_first() {
        return split_0.c_first;
    }
    const auto& c_first() const {
        return split_0.c_first;
    }
    auto& c_middle() {
        return split_0.c_middle;
    }
    const auto& c_middle() const {
        return split_0.c_middle;
    }
    auto& c_last() {
        return split_0.c_last;
    }
    const auto& c_last() const {
        return split_0.c_last;
    }
    auto& c_street_1() {
        return split_0.c_street_1;
    }
    const auto& c_street_1() const {
        return split_0.c_street_1;
    }
    auto& c_street_2() {
        return split_0.c_street_2;
    }
    const auto& c_street_2() const {
        return split_0.c_street_2;
    }
    auto& c_city() {
        return split_0.c_city;
    }
    const auto& c_city() const {
        return split_0.c_city;
    }
    auto& c_state() {
        return split_0.c_state;
    }
    const auto& c_state() const {
        return split_0.c_state;
    }
    auto& c_zip() {
        return split_0.c_zip;
    }
    const auto& c_zip() const {
        return split_0.c_zip;
    }
    auto& c_phone() {
        return split_0.c_phone;
    }
    const auto& c_phone() const {
        return split_0.c_phone;
    }
    auto& c_since() {
        return split_1.c_since;
    }
    const auto& c_since() const {
        return split_1.c_since;
    }
    auto& c_credit() {
        return split_1.c_credit;
    }
    const auto& c_credit() const {
        return split_1.c_credit;
    }
    auto& c_credit_lim() {
        return split_1.c_credit_lim;
    }
    const auto& c_credit_lim() const {
        return split_1.c_credit_lim;
    }
    auto& c_discount() {
        return split_1.c_discount;
    }
    const auto& c_discount() const {
        return split_1.c_discount;
    }
    auto& c_balance() {
        return split_1.c_balance;
    }
    const auto& c_balance() const {
        return split_1.c_balance;
    }
    auto& c_ytd_payment() {
        return split_1.c_ytd_payment;
    }
    const auto& c_ytd_payment() const {
        return split_1.c_ytd_payment;
    }
    auto& c_payment_cnt() {
        return split_1.c_payment_cnt;
    }
    const auto& c_payment_cnt() const {
        return split_1.c_payment_cnt;
    }
    auto& c_delivery_cnt() {
        return split_1.c_delivery_cnt;
    }
    const auto& c_delivery_cnt() const {
        return split_1.c_delivery_cnt;
    }
    auto& c_data() {
        return split_1.c_data;
    }
    const auto& c_data() const {
        return split_1.c_data;
    }

    split_value<0, 9> split_0;
    split_value<9, 18> split_1;
};

template <>
struct split_value<0, 10> {
    explicit split_value() = default;
    accessor<0> c_first;
    accessor<1> c_middle;
    accessor<2> c_last;
    accessor<3> c_street_1;
    accessor<4> c_street_2;
    accessor<5> c_city;
    accessor<6> c_state;
    accessor<7> c_zip;
    accessor<8> c_phone;
    accessor<9> c_since;
};
template <>
struct split_value<10, 18> {
    explicit split_value() = default;
    accessor<10> c_credit;
    accessor<11> c_credit_lim;
    accessor<12> c_discount;
    accessor<13> c_balance;
    accessor<14> c_ytd_payment;
    accessor<15> c_payment_cnt;
    accessor<16> c_delivery_cnt;
    accessor<17> c_data;
};
template <>
struct unified_value<10> {
    auto& c_first() {
        return split_0.c_first;
    }
    const auto& c_first() const {
        return split_0.c_first;
    }
    auto& c_middle() {
        return split_0.c_middle;
    }
    const auto& c_middle() const {
        return split_0.c_middle;
    }
    auto& c_last() {
        return split_0.c_last;
    }
    const auto& c_last() const {
        return split_0.c_last;
    }
    auto& c_street_1() {
        return split_0.c_street_1;
    }
    const auto& c_street_1() const {
        return split_0.c_street_1;
    }
    auto& c_street_2() {
        return split_0.c_street_2;
    }
    const auto& c_street_2() const {
        return split_0.c_street_2;
    }
    auto& c_city() {
        return split_0.c_city;
    }
    const auto& c_city() const {
        return split_0.c_city;
    }
    auto& c_state() {
        return split_0.c_state;
    }
    const auto& c_state() const {
        return split_0.c_state;
    }
    auto& c_zip() {
        return split_0.c_zip;
    }
    const auto& c_zip() const {
        return split_0.c_zip;
    }
    auto& c_phone() {
        return split_0.c_phone;
    }
    const auto& c_phone() const {
        return split_0.c_phone;
    }
    auto& c_since() {
        return split_0.c_since;
    }
    const auto& c_since() const {
        return split_0.c_since;
    }
    auto& c_credit() {
        return split_1.c_credit;
    }
    const auto& c_credit() const {
        return split_1.c_credit;
    }
    auto& c_credit_lim() {
        return split_1.c_credit_lim;
    }
    const auto& c_credit_lim() const {
        return split_1.c_credit_lim;
    }
    auto& c_discount() {
        return split_1.c_discount;
    }
    const auto& c_discount() const {
        return split_1.c_discount;
    }
    auto& c_balance() {
        return split_1.c_balance;
    }
    const auto& c_balance() const {
        return split_1.c_balance;
    }
    auto& c_ytd_payment() {
        return split_1.c_ytd_payment;
    }
    const auto& c_ytd_payment() const {
        return split_1.c_ytd_payment;
    }
    auto& c_payment_cnt() {
        return split_1.c_payment_cnt;
    }
    const auto& c_payment_cnt() const {
        return split_1.c_payment_cnt;
    }
    auto& c_delivery_cnt() {
        return split_1.c_delivery_cnt;
    }
    const auto& c_delivery_cnt() const {
        return split_1.c_delivery_cnt;
    }
    auto& c_data() {
        return split_1.c_data;
    }
    const auto& c_data() const {
        return split_1.c_data;
    }

    split_value<0, 10> split_0;
    split_value<10, 18> split_1;
};

template <>
struct split_value<0, 11> {
    explicit split_value() = default;
    accessor<0> c_first;
    accessor<1> c_middle;
    accessor<2> c_last;
    accessor<3> c_street_1;
    accessor<4> c_street_2;
    accessor<5> c_city;
    accessor<6> c_state;
    accessor<7> c_zip;
    accessor<8> c_phone;
    accessor<9> c_since;
    accessor<10> c_credit;
};
template <>
struct split_value<11, 18> {
    explicit split_value() = default;
    accessor<11> c_credit_lim;
    accessor<12> c_discount;
    accessor<13> c_balance;
    accessor<14> c_ytd_payment;
    accessor<15> c_payment_cnt;
    accessor<16> c_delivery_cnt;
    accessor<17> c_data;
};
template <>
struct unified_value<11> {
    auto& c_first() {
        return split_0.c_first;
    }
    const auto& c_first() const {
        return split_0.c_first;
    }
    auto& c_middle() {
        return split_0.c_middle;
    }
    const auto& c_middle() const {
        return split_0.c_middle;
    }
    auto& c_last() {
        return split_0.c_last;
    }
    const auto& c_last() const {
        return split_0.c_last;
    }
    auto& c_street_1() {
        return split_0.c_street_1;
    }
    const auto& c_street_1() const {
        return split_0.c_street_1;
    }
    auto& c_street_2() {
        return split_0.c_street_2;
    }
    const auto& c_street_2() const {
        return split_0.c_street_2;
    }
    auto& c_city() {
        return split_0.c_city;
    }
    const auto& c_city() const {
        return split_0.c_city;
    }
    auto& c_state() {
        return split_0.c_state;
    }
    const auto& c_state() const {
        return split_0.c_state;
    }
    auto& c_zip() {
        return split_0.c_zip;
    }
    const auto& c_zip() const {
        return split_0.c_zip;
    }
    auto& c_phone() {
        return split_0.c_phone;
    }
    const auto& c_phone() const {
        return split_0.c_phone;
    }
    auto& c_since() {
        return split_0.c_since;
    }
    const auto& c_since() const {
        return split_0.c_since;
    }
    auto& c_credit() {
        return split_0.c_credit;
    }
    const auto& c_credit() const {
        return split_0.c_credit;
    }
    auto& c_credit_lim() {
        return split_1.c_credit_lim;
    }
    const auto& c_credit_lim() const {
        return split_1.c_credit_lim;
    }
    auto& c_discount() {
        return split_1.c_discount;
    }
    const auto& c_discount() const {
        return split_1.c_discount;
    }
    auto& c_balance() {
        return split_1.c_balance;
    }
    const auto& c_balance() const {
        return split_1.c_balance;
    }
    auto& c_ytd_payment() {
        return split_1.c_ytd_payment;
    }
    const auto& c_ytd_payment() const {
        return split_1.c_ytd_payment;
    }
    auto& c_payment_cnt() {
        return split_1.c_payment_cnt;
    }
    const auto& c_payment_cnt() const {
        return split_1.c_payment_cnt;
    }
    auto& c_delivery_cnt() {
        return split_1.c_delivery_cnt;
    }
    const auto& c_delivery_cnt() const {
        return split_1.c_delivery_cnt;
    }
    auto& c_data() {
        return split_1.c_data;
    }
    const auto& c_data() const {
        return split_1.c_data;
    }

    split_value<0, 11> split_0;
    split_value<11, 18> split_1;
};

template <>
struct split_value<0, 12> {
    explicit split_value() = default;
    accessor<0> c_first;
    accessor<1> c_middle;
    accessor<2> c_last;
    accessor<3> c_street_1;
    accessor<4> c_street_2;
    accessor<5> c_city;
    accessor<6> c_state;
    accessor<7> c_zip;
    accessor<8> c_phone;
    accessor<9> c_since;
    accessor<10> c_credit;
    accessor<11> c_credit_lim;
};
template <>
struct split_value<12, 18> {
    explicit split_value() = default;
    accessor<12> c_discount;
    accessor<13> c_balance;
    accessor<14> c_ytd_payment;
    accessor<15> c_payment_cnt;
    accessor<16> c_delivery_cnt;
    accessor<17> c_data;
};
template <>
struct unified_value<12> {
    auto& c_first() {
        return split_0.c_first;
    }
    const auto& c_first() const {
        return split_0.c_first;
    }
    auto& c_middle() {
        return split_0.c_middle;
    }
    const auto& c_middle() const {
        return split_0.c_middle;
    }
    auto& c_last() {
        return split_0.c_last;
    }
    const auto& c_last() const {
        return split_0.c_last;
    }
    auto& c_street_1() {
        return split_0.c_street_1;
    }
    const auto& c_street_1() const {
        return split_0.c_street_1;
    }
    auto& c_street_2() {
        return split_0.c_street_2;
    }
    const auto& c_street_2() const {
        return split_0.c_street_2;
    }
    auto& c_city() {
        return split_0.c_city;
    }
    const auto& c_city() const {
        return split_0.c_city;
    }
    auto& c_state() {
        return split_0.c_state;
    }
    const auto& c_state() const {
        return split_0.c_state;
    }
    auto& c_zip() {
        return split_0.c_zip;
    }
    const auto& c_zip() const {
        return split_0.c_zip;
    }
    auto& c_phone() {
        return split_0.c_phone;
    }
    const auto& c_phone() const {
        return split_0.c_phone;
    }
    auto& c_since() {
        return split_0.c_since;
    }
    const auto& c_since() const {
        return split_0.c_since;
    }
    auto& c_credit() {
        return split_0.c_credit;
    }
    const auto& c_credit() const {
        return split_0.c_credit;
    }
    auto& c_credit_lim() {
        return split_0.c_credit_lim;
    }
    const auto& c_credit_lim() const {
        return split_0.c_credit_lim;
    }
    auto& c_discount() {
        return split_1.c_discount;
    }
    const auto& c_discount() const {
        return split_1.c_discount;
    }
    auto& c_balance() {
        return split_1.c_balance;
    }
    const auto& c_balance() const {
        return split_1.c_balance;
    }
    auto& c_ytd_payment() {
        return split_1.c_ytd_payment;
    }
    const auto& c_ytd_payment() const {
        return split_1.c_ytd_payment;
    }
    auto& c_payment_cnt() {
        return split_1.c_payment_cnt;
    }
    const auto& c_payment_cnt() const {
        return split_1.c_payment_cnt;
    }
    auto& c_delivery_cnt() {
        return split_1.c_delivery_cnt;
    }
    const auto& c_delivery_cnt() const {
        return split_1.c_delivery_cnt;
    }
    auto& c_data() {
        return split_1.c_data;
    }
    const auto& c_data() const {
        return split_1.c_data;
    }

    split_value<0, 12> split_0;
    split_value<12, 18> split_1;
};

template <>
struct split_value<0, 13> {
    explicit split_value() = default;
    accessor<0> c_first;
    accessor<1> c_middle;
    accessor<2> c_last;
    accessor<3> c_street_1;
    accessor<4> c_street_2;
    accessor<5> c_city;
    accessor<6> c_state;
    accessor<7> c_zip;
    accessor<8> c_phone;
    accessor<9> c_since;
    accessor<10> c_credit;
    accessor<11> c_credit_lim;
    accessor<12> c_discount;
};
template <>
struct split_value<13, 18> {
    explicit split_value() = default;
    accessor<13> c_balance;
    accessor<14> c_ytd_payment;
    accessor<15> c_payment_cnt;
    accessor<16> c_delivery_cnt;
    accessor<17> c_data;
};
template <>
struct unified_value<13> {
    auto& c_first() {
        return split_0.c_first;
    }
    const auto& c_first() const {
        return split_0.c_first;
    }
    auto& c_middle() {
        return split_0.c_middle;
    }
    const auto& c_middle() const {
        return split_0.c_middle;
    }
    auto& c_last() {
        return split_0.c_last;
    }
    const auto& c_last() const {
        return split_0.c_last;
    }
    auto& c_street_1() {
        return split_0.c_street_1;
    }
    const auto& c_street_1() const {
        return split_0.c_street_1;
    }
    auto& c_street_2() {
        return split_0.c_street_2;
    }
    const auto& c_street_2() const {
        return split_0.c_street_2;
    }
    auto& c_city() {
        return split_0.c_city;
    }
    const auto& c_city() const {
        return split_0.c_city;
    }
    auto& c_state() {
        return split_0.c_state;
    }
    const auto& c_state() const {
        return split_0.c_state;
    }
    auto& c_zip() {
        return split_0.c_zip;
    }
    const auto& c_zip() const {
        return split_0.c_zip;
    }
    auto& c_phone() {
        return split_0.c_phone;
    }
    const auto& c_phone() const {
        return split_0.c_phone;
    }
    auto& c_since() {
        return split_0.c_since;
    }
    const auto& c_since() const {
        return split_0.c_since;
    }
    auto& c_credit() {
        return split_0.c_credit;
    }
    const auto& c_credit() const {
        return split_0.c_credit;
    }
    auto& c_credit_lim() {
        return split_0.c_credit_lim;
    }
    const auto& c_credit_lim() const {
        return split_0.c_credit_lim;
    }
    auto& c_discount() {
        return split_0.c_discount;
    }
    const auto& c_discount() const {
        return split_0.c_discount;
    }
    auto& c_balance() {
        return split_1.c_balance;
    }
    const auto& c_balance() const {
        return split_1.c_balance;
    }
    auto& c_ytd_payment() {
        return split_1.c_ytd_payment;
    }
    const auto& c_ytd_payment() const {
        return split_1.c_ytd_payment;
    }
    auto& c_payment_cnt() {
        return split_1.c_payment_cnt;
    }
    const auto& c_payment_cnt() const {
        return split_1.c_payment_cnt;
    }
    auto& c_delivery_cnt() {
        return split_1.c_delivery_cnt;
    }
    const auto& c_delivery_cnt() const {
        return split_1.c_delivery_cnt;
    }
    auto& c_data() {
        return split_1.c_data;
    }
    const auto& c_data() const {
        return split_1.c_data;
    }

    split_value<0, 13> split_0;
    split_value<13, 18> split_1;
};

template <>
struct split_value<0, 14> {
    explicit split_value() = default;
    accessor<0> c_first;
    accessor<1> c_middle;
    accessor<2> c_last;
    accessor<3> c_street_1;
    accessor<4> c_street_2;
    accessor<5> c_city;
    accessor<6> c_state;
    accessor<7> c_zip;
    accessor<8> c_phone;
    accessor<9> c_since;
    accessor<10> c_credit;
    accessor<11> c_credit_lim;
    accessor<12> c_discount;
    accessor<13> c_balance;
};
template <>
struct split_value<14, 18> {
    explicit split_value() = default;
    accessor<14> c_ytd_payment;
    accessor<15> c_payment_cnt;
    accessor<16> c_delivery_cnt;
    accessor<17> c_data;
};
template <>
struct unified_value<14> {
    auto& c_first() {
        return split_0.c_first;
    }
    const auto& c_first() const {
        return split_0.c_first;
    }
    auto& c_middle() {
        return split_0.c_middle;
    }
    const auto& c_middle() const {
        return split_0.c_middle;
    }
    auto& c_last() {
        return split_0.c_last;
    }
    const auto& c_last() const {
        return split_0.c_last;
    }
    auto& c_street_1() {
        return split_0.c_street_1;
    }
    const auto& c_street_1() const {
        return split_0.c_street_1;
    }
    auto& c_street_2() {
        return split_0.c_street_2;
    }
    const auto& c_street_2() const {
        return split_0.c_street_2;
    }
    auto& c_city() {
        return split_0.c_city;
    }
    const auto& c_city() const {
        return split_0.c_city;
    }
    auto& c_state() {
        return split_0.c_state;
    }
    const auto& c_state() const {
        return split_0.c_state;
    }
    auto& c_zip() {
        return split_0.c_zip;
    }
    const auto& c_zip() const {
        return split_0.c_zip;
    }
    auto& c_phone() {
        return split_0.c_phone;
    }
    const auto& c_phone() const {
        return split_0.c_phone;
    }
    auto& c_since() {
        return split_0.c_since;
    }
    const auto& c_since() const {
        return split_0.c_since;
    }
    auto& c_credit() {
        return split_0.c_credit;
    }
    const auto& c_credit() const {
        return split_0.c_credit;
    }
    auto& c_credit_lim() {
        return split_0.c_credit_lim;
    }
    const auto& c_credit_lim() const {
        return split_0.c_credit_lim;
    }
    auto& c_discount() {
        return split_0.c_discount;
    }
    const auto& c_discount() const {
        return split_0.c_discount;
    }
    auto& c_balance() {
        return split_0.c_balance;
    }
    const auto& c_balance() const {
        return split_0.c_balance;
    }
    auto& c_ytd_payment() {
        return split_1.c_ytd_payment;
    }
    const auto& c_ytd_payment() const {
        return split_1.c_ytd_payment;
    }
    auto& c_payment_cnt() {
        return split_1.c_payment_cnt;
    }
    const auto& c_payment_cnt() const {
        return split_1.c_payment_cnt;
    }
    auto& c_delivery_cnt() {
        return split_1.c_delivery_cnt;
    }
    const auto& c_delivery_cnt() const {
        return split_1.c_delivery_cnt;
    }
    auto& c_data() {
        return split_1.c_data;
    }
    const auto& c_data() const {
        return split_1.c_data;
    }

    split_value<0, 14> split_0;
    split_value<14, 18> split_1;
};

template <>
struct split_value<0, 15> {
    explicit split_value() = default;
    accessor<0> c_first;
    accessor<1> c_middle;
    accessor<2> c_last;
    accessor<3> c_street_1;
    accessor<4> c_street_2;
    accessor<5> c_city;
    accessor<6> c_state;
    accessor<7> c_zip;
    accessor<8> c_phone;
    accessor<9> c_since;
    accessor<10> c_credit;
    accessor<11> c_credit_lim;
    accessor<12> c_discount;
    accessor<13> c_balance;
    accessor<14> c_ytd_payment;
};
template <>
struct split_value<15, 18> {
    explicit split_value() = default;
    accessor<15> c_payment_cnt;
    accessor<16> c_delivery_cnt;
    accessor<17> c_data;
};
template <>
struct unified_value<15> {
    auto& c_first() {
        return split_0.c_first;
    }
    const auto& c_first() const {
        return split_0.c_first;
    }
    auto& c_middle() {
        return split_0.c_middle;
    }
    const auto& c_middle() const {
        return split_0.c_middle;
    }
    auto& c_last() {
        return split_0.c_last;
    }
    const auto& c_last() const {
        return split_0.c_last;
    }
    auto& c_street_1() {
        return split_0.c_street_1;
    }
    const auto& c_street_1() const {
        return split_0.c_street_1;
    }
    auto& c_street_2() {
        return split_0.c_street_2;
    }
    const auto& c_street_2() const {
        return split_0.c_street_2;
    }
    auto& c_city() {
        return split_0.c_city;
    }
    const auto& c_city() const {
        return split_0.c_city;
    }
    auto& c_state() {
        return split_0.c_state;
    }
    const auto& c_state() const {
        return split_0.c_state;
    }
    auto& c_zip() {
        return split_0.c_zip;
    }
    const auto& c_zip() const {
        return split_0.c_zip;
    }
    auto& c_phone() {
        return split_0.c_phone;
    }
    const auto& c_phone() const {
        return split_0.c_phone;
    }
    auto& c_since() {
        return split_0.c_since;
    }
    const auto& c_since() const {
        return split_0.c_since;
    }
    auto& c_credit() {
        return split_0.c_credit;
    }
    const auto& c_credit() const {
        return split_0.c_credit;
    }
    auto& c_credit_lim() {
        return split_0.c_credit_lim;
    }
    const auto& c_credit_lim() const {
        return split_0.c_credit_lim;
    }
    auto& c_discount() {
        return split_0.c_discount;
    }
    const auto& c_discount() const {
        return split_0.c_discount;
    }
    auto& c_balance() {
        return split_0.c_balance;
    }
    const auto& c_balance() const {
        return split_0.c_balance;
    }
    auto& c_ytd_payment() {
        return split_0.c_ytd_payment;
    }
    const auto& c_ytd_payment() const {
        return split_0.c_ytd_payment;
    }
    auto& c_payment_cnt() {
        return split_1.c_payment_cnt;
    }
    const auto& c_payment_cnt() const {
        return split_1.c_payment_cnt;
    }
    auto& c_delivery_cnt() {
        return split_1.c_delivery_cnt;
    }
    const auto& c_delivery_cnt() const {
        return split_1.c_delivery_cnt;
    }
    auto& c_data() {
        return split_1.c_data;
    }
    const auto& c_data() const {
        return split_1.c_data;
    }

    split_value<0, 15> split_0;
    split_value<15, 18> split_1;
};

template <>
struct split_value<0, 16> {
    explicit split_value() = default;
    accessor<0> c_first;
    accessor<1> c_middle;
    accessor<2> c_last;
    accessor<3> c_street_1;
    accessor<4> c_street_2;
    accessor<5> c_city;
    accessor<6> c_state;
    accessor<7> c_zip;
    accessor<8> c_phone;
    accessor<9> c_since;
    accessor<10> c_credit;
    accessor<11> c_credit_lim;
    accessor<12> c_discount;
    accessor<13> c_balance;
    accessor<14> c_ytd_payment;
    accessor<15> c_payment_cnt;
};
template <>
struct split_value<16, 18> {
    explicit split_value() = default;
    accessor<16> c_delivery_cnt;
    accessor<17> c_data;
};
template <>
struct unified_value<16> {
    auto& c_first() {
        return split_0.c_first;
    }
    const auto& c_first() const {
        return split_0.c_first;
    }
    auto& c_middle() {
        return split_0.c_middle;
    }
    const auto& c_middle() const {
        return split_0.c_middle;
    }
    auto& c_last() {
        return split_0.c_last;
    }
    const auto& c_last() const {
        return split_0.c_last;
    }
    auto& c_street_1() {
        return split_0.c_street_1;
    }
    const auto& c_street_1() const {
        return split_0.c_street_1;
    }
    auto& c_street_2() {
        return split_0.c_street_2;
    }
    const auto& c_street_2() const {
        return split_0.c_street_2;
    }
    auto& c_city() {
        return split_0.c_city;
    }
    const auto& c_city() const {
        return split_0.c_city;
    }
    auto& c_state() {
        return split_0.c_state;
    }
    const auto& c_state() const {
        return split_0.c_state;
    }
    auto& c_zip() {
        return split_0.c_zip;
    }
    const auto& c_zip() const {
        return split_0.c_zip;
    }
    auto& c_phone() {
        return split_0.c_phone;
    }
    const auto& c_phone() const {
        return split_0.c_phone;
    }
    auto& c_since() {
        return split_0.c_since;
    }
    const auto& c_since() const {
        return split_0.c_since;
    }
    auto& c_credit() {
        return split_0.c_credit;
    }
    const auto& c_credit() const {
        return split_0.c_credit;
    }
    auto& c_credit_lim() {
        return split_0.c_credit_lim;
    }
    const auto& c_credit_lim() const {
        return split_0.c_credit_lim;
    }
    auto& c_discount() {
        return split_0.c_discount;
    }
    const auto& c_discount() const {
        return split_0.c_discount;
    }
    auto& c_balance() {
        return split_0.c_balance;
    }
    const auto& c_balance() const {
        return split_0.c_balance;
    }
    auto& c_ytd_payment() {
        return split_0.c_ytd_payment;
    }
    const auto& c_ytd_payment() const {
        return split_0.c_ytd_payment;
    }
    auto& c_payment_cnt() {
        return split_0.c_payment_cnt;
    }
    const auto& c_payment_cnt() const {
        return split_0.c_payment_cnt;
    }
    auto& c_delivery_cnt() {
        return split_1.c_delivery_cnt;
    }
    const auto& c_delivery_cnt() const {
        return split_1.c_delivery_cnt;
    }
    auto& c_data() {
        return split_1.c_data;
    }
    const auto& c_data() const {
        return split_1.c_data;
    }

    split_value<0, 16> split_0;
    split_value<16, 18> split_1;
};

template <>
struct split_value<0, 17> {
    explicit split_value() = default;
    accessor<0> c_first;
    accessor<1> c_middle;
    accessor<2> c_last;
    accessor<3> c_street_1;
    accessor<4> c_street_2;
    accessor<5> c_city;
    accessor<6> c_state;
    accessor<7> c_zip;
    accessor<8> c_phone;
    accessor<9> c_since;
    accessor<10> c_credit;
    accessor<11> c_credit_lim;
    accessor<12> c_discount;
    accessor<13> c_balance;
    accessor<14> c_ytd_payment;
    accessor<15> c_payment_cnt;
    accessor<16> c_delivery_cnt;
};
template <>
struct split_value<17, 18> {
    explicit split_value() = default;
    accessor<17> c_data;
};
template <>
struct unified_value<17> {
    auto& c_first() {
        return split_0.c_first;
    }
    const auto& c_first() const {
        return split_0.c_first;
    }
    auto& c_middle() {
        return split_0.c_middle;
    }
    const auto& c_middle() const {
        return split_0.c_middle;
    }
    auto& c_last() {
        return split_0.c_last;
    }
    const auto& c_last() const {
        return split_0.c_last;
    }
    auto& c_street_1() {
        return split_0.c_street_1;
    }
    const auto& c_street_1() const {
        return split_0.c_street_1;
    }
    auto& c_street_2() {
        return split_0.c_street_2;
    }
    const auto& c_street_2() const {
        return split_0.c_street_2;
    }
    auto& c_city() {
        return split_0.c_city;
    }
    const auto& c_city() const {
        return split_0.c_city;
    }
    auto& c_state() {
        return split_0.c_state;
    }
    const auto& c_state() const {
        return split_0.c_state;
    }
    auto& c_zip() {
        return split_0.c_zip;
    }
    const auto& c_zip() const {
        return split_0.c_zip;
    }
    auto& c_phone() {
        return split_0.c_phone;
    }
    const auto& c_phone() const {
        return split_0.c_phone;
    }
    auto& c_since() {
        return split_0.c_since;
    }
    const auto& c_since() const {
        return split_0.c_since;
    }
    auto& c_credit() {
        return split_0.c_credit;
    }
    const auto& c_credit() const {
        return split_0.c_credit;
    }
    auto& c_credit_lim() {
        return split_0.c_credit_lim;
    }
    const auto& c_credit_lim() const {
        return split_0.c_credit_lim;
    }
    auto& c_discount() {
        return split_0.c_discount;
    }
    const auto& c_discount() const {
        return split_0.c_discount;
    }
    auto& c_balance() {
        return split_0.c_balance;
    }
    const auto& c_balance() const {
        return split_0.c_balance;
    }
    auto& c_ytd_payment() {
        return split_0.c_ytd_payment;
    }
    const auto& c_ytd_payment() const {
        return split_0.c_ytd_payment;
    }
    auto& c_payment_cnt() {
        return split_0.c_payment_cnt;
    }
    const auto& c_payment_cnt() const {
        return split_0.c_payment_cnt;
    }
    auto& c_delivery_cnt() {
        return split_0.c_delivery_cnt;
    }
    const auto& c_delivery_cnt() const {
        return split_0.c_delivery_cnt;
    }
    auto& c_data() {
        return split_1.c_data;
    }
    const auto& c_data() const {
        return split_1.c_data;
    }

    split_value<0, 17> split_0;
    split_value<17, 18> split_1;
};

template <>
struct split_value<0, 18> {
    explicit split_value() = default;
    accessor<0> c_first;
    accessor<1> c_middle;
    accessor<2> c_last;
    accessor<3> c_street_1;
    accessor<4> c_street_2;
    accessor<5> c_city;
    accessor<6> c_state;
    accessor<7> c_zip;
    accessor<8> c_phone;
    accessor<9> c_since;
    accessor<10> c_credit;
    accessor<11> c_credit_lim;
    accessor<12> c_discount;
    accessor<13> c_balance;
    accessor<14> c_ytd_payment;
    accessor<15> c_payment_cnt;
    accessor<16> c_delivery_cnt;
    accessor<17> c_data;
};
template <>
struct unified_value<18> {
    auto& c_first() {
        return split_0.c_first;
    }
    const auto& c_first() const {
        return split_0.c_first;
    }
    auto& c_middle() {
        return split_0.c_middle;
    }
    const auto& c_middle() const {
        return split_0.c_middle;
    }
    auto& c_last() {
        return split_0.c_last;
    }
    const auto& c_last() const {
        return split_0.c_last;
    }
    auto& c_street_1() {
        return split_0.c_street_1;
    }
    const auto& c_street_1() const {
        return split_0.c_street_1;
    }
    auto& c_street_2() {
        return split_0.c_street_2;
    }
    const auto& c_street_2() const {
        return split_0.c_street_2;
    }
    auto& c_city() {
        return split_0.c_city;
    }
    const auto& c_city() const {
        return split_0.c_city;
    }
    auto& c_state() {
        return split_0.c_state;
    }
    const auto& c_state() const {
        return split_0.c_state;
    }
    auto& c_zip() {
        return split_0.c_zip;
    }
    const auto& c_zip() const {
        return split_0.c_zip;
    }
    auto& c_phone() {
        return split_0.c_phone;
    }
    const auto& c_phone() const {
        return split_0.c_phone;
    }
    auto& c_since() {
        return split_0.c_since;
    }
    const auto& c_since() const {
        return split_0.c_since;
    }
    auto& c_credit() {
        return split_0.c_credit;
    }
    const auto& c_credit() const {
        return split_0.c_credit;
    }
    auto& c_credit_lim() {
        return split_0.c_credit_lim;
    }
    const auto& c_credit_lim() const {
        return split_0.c_credit_lim;
    }
    auto& c_discount() {
        return split_0.c_discount;
    }
    const auto& c_discount() const {
        return split_0.c_discount;
    }
    auto& c_balance() {
        return split_0.c_balance;
    }
    const auto& c_balance() const {
        return split_0.c_balance;
    }
    auto& c_ytd_payment() {
        return split_0.c_ytd_payment;
    }
    const auto& c_ytd_payment() const {
        return split_0.c_ytd_payment;
    }
    auto& c_payment_cnt() {
        return split_0.c_payment_cnt;
    }
    const auto& c_payment_cnt() const {
        return split_0.c_payment_cnt;
    }
    auto& c_delivery_cnt() {
        return split_0.c_delivery_cnt;
    }
    const auto& c_delivery_cnt() const {
        return split_0.c_delivery_cnt;
    }
    auto& c_data() {
        return split_0.c_data;
    }
    const auto& c_data() const {
        return split_0.c_data;
    }

    split_value<0, 18> split_0;
};

struct customer_value {
    explicit customer_value() = default;

    using NamedColumn = customer_value_datatypes::NamedColumn;

    auto& c_first() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.c_first();
            }, value);
    }
    const auto& c_first() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.c_first();
            }, value);
    }
    auto& c_middle() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.c_middle();
            }, value);
    }
    const auto& c_middle() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.c_middle();
            }, value);
    }
    auto& c_last() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.c_last();
            }, value);
    }
    const auto& c_last() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.c_last();
            }, value);
    }
    auto& c_street_1() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.c_street_1();
            }, value);
    }
    const auto& c_street_1() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.c_street_1();
            }, value);
    }
    auto& c_street_2() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.c_street_2();
            }, value);
    }
    const auto& c_street_2() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.c_street_2();
            }, value);
    }
    auto& c_city() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.c_city();
            }, value);
    }
    const auto& c_city() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.c_city();
            }, value);
    }
    auto& c_state() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.c_state();
            }, value);
    }
    const auto& c_state() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.c_state();
            }, value);
    }
    auto& c_zip() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.c_zip();
            }, value);
    }
    const auto& c_zip() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.c_zip();
            }, value);
    }
    auto& c_phone() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.c_phone();
            }, value);
    }
    const auto& c_phone() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.c_phone();
            }, value);
    }
    auto& c_since() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.c_since();
            }, value);
    }
    const auto& c_since() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.c_since();
            }, value);
    }
    auto& c_credit() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.c_credit();
            }, value);
    }
    const auto& c_credit() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.c_credit();
            }, value);
    }
    auto& c_credit_lim() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.c_credit_lim();
            }, value);
    }
    const auto& c_credit_lim() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.c_credit_lim();
            }, value);
    }
    auto& c_discount() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.c_discount();
            }, value);
    }
    const auto& c_discount() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.c_discount();
            }, value);
    }
    auto& c_balance() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.c_balance();
            }, value);
    }
    const auto& c_balance() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.c_balance();
            }, value);
    }
    auto& c_ytd_payment() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.c_ytd_payment();
            }, value);
    }
    const auto& c_ytd_payment() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.c_ytd_payment();
            }, value);
    }
    auto& c_payment_cnt() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.c_payment_cnt();
            }, value);
    }
    const auto& c_payment_cnt() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.c_payment_cnt();
            }, value);
    }
    auto& c_delivery_cnt() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.c_delivery_cnt();
            }, value);
    }
    const auto& c_delivery_cnt() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.c_delivery_cnt();
            }, value);
    }
    auto& c_data() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.c_data();
            }, value);
    }
    const auto& c_data() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.c_data();
            }, value);
    }

    std::variant<
        unified_value<18>,
        unified_value<17>,
        unified_value<16>,
        unified_value<15>,
        unified_value<14>,
        unified_value<13>,
        unified_value<12>,
        unified_value<11>,
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
};  // namespace customer_value_datatypes

using customer_value = customer_value_datatypes::customer_value;
using ADAPTER_OF(customer_value) = ADAPTER_OF(customer_value_datatypes::customer_value);

namespace history_value_datatypes {

enum class NamedColumn : int {
    h_c_id = 0,
    h_c_d_id = 1,
    h_c_w_id = 2,
    h_d_id = 3,
    h_w_id = 4,
    h_date = 5,
    h_amount = 6,
    h_data = 7,
    COLCOUNT = 8
};

template <size_t ColIndex>
struct accessor;

template <size_t StartIndex, size_t EndIndex>
struct split_value;

template <size_t SplitIndex>
struct unified_value;

struct history_value;

DEFINE_ADAPTER(history_value, 8);

template <size_t ColIndex>
struct accessor_info;

template <>
struct accessor_info<0> {
    using type = uint64_t;
};

template <>
struct accessor_info<1> {
    using type = uint64_t;
};

template <>
struct accessor_info<2> {
    using type = uint64_t;
};

template <>
struct accessor_info<3> {
    using type = uint64_t;
};

template <>
struct accessor_info<4> {
    using type = uint64_t;
};

template <>
struct accessor_info<5> {
    using type = uint32_t;
};

template <>
struct accessor_info<6> {
    using type = int64_t;
};

template <>
struct accessor_info<7> {
    using type = var_string<24>;
};

template <size_t ColIndex>
struct accessor {
    using type = typename accessor_info<ColIndex>::type;

    accessor() = default;
    accessor(type& value) : value_(value) {}
    accessor(const type& value) : value_(const_cast<type&>(value)) {}

    operator type() {
        ADAPTER_OF(history_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    operator const type() const {
        ADAPTER_OF(history_value)::CountRead(ColIndex + index_);
        return value_;
    }

    operator type&() {
        ADAPTER_OF(history_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    operator const type&() const {
        ADAPTER_OF(history_value)::CountRead(ColIndex + index_);
        return value_;
    }

    type operator =(const type& other) {
        ADAPTER_OF(history_value)::CountWrite(ColIndex + index_);
        return value_ = other;
    }

    type operator =(const accessor<ColIndex>& other) {
        ADAPTER_OF(history_value)::CountWrite(ColIndex + index_);
        return value_ = other.value_;
    }

    type operator *() {
        ADAPTER_OF(history_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    const type operator *() const {
        ADAPTER_OF(history_value)::CountRead(ColIndex + index_);
        return value_;
    }

    type* operator ->() {
        ADAPTER_OF(history_value)::CountWrite(ColIndex + index_);
        return &value_;
    }

    const type* operator ->() const {
        ADAPTER_OF(history_value)::CountRead(ColIndex + index_);
        return &value_;
    }

    size_t index_ = 0;
    type value_;
};

template <>
struct split_value<0, 1> {
    explicit split_value() = default;
    accessor<0> h_c_id;
};
template <>
struct split_value<1, 8> {
    explicit split_value() = default;
    accessor<1> h_c_d_id;
    accessor<2> h_c_w_id;
    accessor<3> h_d_id;
    accessor<4> h_w_id;
    accessor<5> h_date;
    accessor<6> h_amount;
    accessor<7> h_data;
};
template <>
struct unified_value<1> {
    auto& h_c_id() {
        return split_0.h_c_id;
    }
    const auto& h_c_id() const {
        return split_0.h_c_id;
    }
    auto& h_c_d_id() {
        return split_1.h_c_d_id;
    }
    const auto& h_c_d_id() const {
        return split_1.h_c_d_id;
    }
    auto& h_c_w_id() {
        return split_1.h_c_w_id;
    }
    const auto& h_c_w_id() const {
        return split_1.h_c_w_id;
    }
    auto& h_d_id() {
        return split_1.h_d_id;
    }
    const auto& h_d_id() const {
        return split_1.h_d_id;
    }
    auto& h_w_id() {
        return split_1.h_w_id;
    }
    const auto& h_w_id() const {
        return split_1.h_w_id;
    }
    auto& h_date() {
        return split_1.h_date;
    }
    const auto& h_date() const {
        return split_1.h_date;
    }
    auto& h_amount() {
        return split_1.h_amount;
    }
    const auto& h_amount() const {
        return split_1.h_amount;
    }
    auto& h_data() {
        return split_1.h_data;
    }
    const auto& h_data() const {
        return split_1.h_data;
    }

    split_value<0, 1> split_0;
    split_value<1, 8> split_1;
};

template <>
struct split_value<0, 2> {
    explicit split_value() = default;
    accessor<0> h_c_id;
    accessor<1> h_c_d_id;
};
template <>
struct split_value<2, 8> {
    explicit split_value() = default;
    accessor<2> h_c_w_id;
    accessor<3> h_d_id;
    accessor<4> h_w_id;
    accessor<5> h_date;
    accessor<6> h_amount;
    accessor<7> h_data;
};
template <>
struct unified_value<2> {
    auto& h_c_id() {
        return split_0.h_c_id;
    }
    const auto& h_c_id() const {
        return split_0.h_c_id;
    }
    auto& h_c_d_id() {
        return split_0.h_c_d_id;
    }
    const auto& h_c_d_id() const {
        return split_0.h_c_d_id;
    }
    auto& h_c_w_id() {
        return split_1.h_c_w_id;
    }
    const auto& h_c_w_id() const {
        return split_1.h_c_w_id;
    }
    auto& h_d_id() {
        return split_1.h_d_id;
    }
    const auto& h_d_id() const {
        return split_1.h_d_id;
    }
    auto& h_w_id() {
        return split_1.h_w_id;
    }
    const auto& h_w_id() const {
        return split_1.h_w_id;
    }
    auto& h_date() {
        return split_1.h_date;
    }
    const auto& h_date() const {
        return split_1.h_date;
    }
    auto& h_amount() {
        return split_1.h_amount;
    }
    const auto& h_amount() const {
        return split_1.h_amount;
    }
    auto& h_data() {
        return split_1.h_data;
    }
    const auto& h_data() const {
        return split_1.h_data;
    }

    split_value<0, 2> split_0;
    split_value<2, 8> split_1;
};

template <>
struct split_value<0, 3> {
    explicit split_value() = default;
    accessor<0> h_c_id;
    accessor<1> h_c_d_id;
    accessor<2> h_c_w_id;
};
template <>
struct split_value<3, 8> {
    explicit split_value() = default;
    accessor<3> h_d_id;
    accessor<4> h_w_id;
    accessor<5> h_date;
    accessor<6> h_amount;
    accessor<7> h_data;
};
template <>
struct unified_value<3> {
    auto& h_c_id() {
        return split_0.h_c_id;
    }
    const auto& h_c_id() const {
        return split_0.h_c_id;
    }
    auto& h_c_d_id() {
        return split_0.h_c_d_id;
    }
    const auto& h_c_d_id() const {
        return split_0.h_c_d_id;
    }
    auto& h_c_w_id() {
        return split_0.h_c_w_id;
    }
    const auto& h_c_w_id() const {
        return split_0.h_c_w_id;
    }
    auto& h_d_id() {
        return split_1.h_d_id;
    }
    const auto& h_d_id() const {
        return split_1.h_d_id;
    }
    auto& h_w_id() {
        return split_1.h_w_id;
    }
    const auto& h_w_id() const {
        return split_1.h_w_id;
    }
    auto& h_date() {
        return split_1.h_date;
    }
    const auto& h_date() const {
        return split_1.h_date;
    }
    auto& h_amount() {
        return split_1.h_amount;
    }
    const auto& h_amount() const {
        return split_1.h_amount;
    }
    auto& h_data() {
        return split_1.h_data;
    }
    const auto& h_data() const {
        return split_1.h_data;
    }

    split_value<0, 3> split_0;
    split_value<3, 8> split_1;
};

template <>
struct split_value<0, 4> {
    explicit split_value() = default;
    accessor<0> h_c_id;
    accessor<1> h_c_d_id;
    accessor<2> h_c_w_id;
    accessor<3> h_d_id;
};
template <>
struct split_value<4, 8> {
    explicit split_value() = default;
    accessor<4> h_w_id;
    accessor<5> h_date;
    accessor<6> h_amount;
    accessor<7> h_data;
};
template <>
struct unified_value<4> {
    auto& h_c_id() {
        return split_0.h_c_id;
    }
    const auto& h_c_id() const {
        return split_0.h_c_id;
    }
    auto& h_c_d_id() {
        return split_0.h_c_d_id;
    }
    const auto& h_c_d_id() const {
        return split_0.h_c_d_id;
    }
    auto& h_c_w_id() {
        return split_0.h_c_w_id;
    }
    const auto& h_c_w_id() const {
        return split_0.h_c_w_id;
    }
    auto& h_d_id() {
        return split_0.h_d_id;
    }
    const auto& h_d_id() const {
        return split_0.h_d_id;
    }
    auto& h_w_id() {
        return split_1.h_w_id;
    }
    const auto& h_w_id() const {
        return split_1.h_w_id;
    }
    auto& h_date() {
        return split_1.h_date;
    }
    const auto& h_date() const {
        return split_1.h_date;
    }
    auto& h_amount() {
        return split_1.h_amount;
    }
    const auto& h_amount() const {
        return split_1.h_amount;
    }
    auto& h_data() {
        return split_1.h_data;
    }
    const auto& h_data() const {
        return split_1.h_data;
    }

    split_value<0, 4> split_0;
    split_value<4, 8> split_1;
};

template <>
struct split_value<0, 5> {
    explicit split_value() = default;
    accessor<0> h_c_id;
    accessor<1> h_c_d_id;
    accessor<2> h_c_w_id;
    accessor<3> h_d_id;
    accessor<4> h_w_id;
};
template <>
struct split_value<5, 8> {
    explicit split_value() = default;
    accessor<5> h_date;
    accessor<6> h_amount;
    accessor<7> h_data;
};
template <>
struct unified_value<5> {
    auto& h_c_id() {
        return split_0.h_c_id;
    }
    const auto& h_c_id() const {
        return split_0.h_c_id;
    }
    auto& h_c_d_id() {
        return split_0.h_c_d_id;
    }
    const auto& h_c_d_id() const {
        return split_0.h_c_d_id;
    }
    auto& h_c_w_id() {
        return split_0.h_c_w_id;
    }
    const auto& h_c_w_id() const {
        return split_0.h_c_w_id;
    }
    auto& h_d_id() {
        return split_0.h_d_id;
    }
    const auto& h_d_id() const {
        return split_0.h_d_id;
    }
    auto& h_w_id() {
        return split_0.h_w_id;
    }
    const auto& h_w_id() const {
        return split_0.h_w_id;
    }
    auto& h_date() {
        return split_1.h_date;
    }
    const auto& h_date() const {
        return split_1.h_date;
    }
    auto& h_amount() {
        return split_1.h_amount;
    }
    const auto& h_amount() const {
        return split_1.h_amount;
    }
    auto& h_data() {
        return split_1.h_data;
    }
    const auto& h_data() const {
        return split_1.h_data;
    }

    split_value<0, 5> split_0;
    split_value<5, 8> split_1;
};

template <>
struct split_value<0, 6> {
    explicit split_value() = default;
    accessor<0> h_c_id;
    accessor<1> h_c_d_id;
    accessor<2> h_c_w_id;
    accessor<3> h_d_id;
    accessor<4> h_w_id;
    accessor<5> h_date;
};
template <>
struct split_value<6, 8> {
    explicit split_value() = default;
    accessor<6> h_amount;
    accessor<7> h_data;
};
template <>
struct unified_value<6> {
    auto& h_c_id() {
        return split_0.h_c_id;
    }
    const auto& h_c_id() const {
        return split_0.h_c_id;
    }
    auto& h_c_d_id() {
        return split_0.h_c_d_id;
    }
    const auto& h_c_d_id() const {
        return split_0.h_c_d_id;
    }
    auto& h_c_w_id() {
        return split_0.h_c_w_id;
    }
    const auto& h_c_w_id() const {
        return split_0.h_c_w_id;
    }
    auto& h_d_id() {
        return split_0.h_d_id;
    }
    const auto& h_d_id() const {
        return split_0.h_d_id;
    }
    auto& h_w_id() {
        return split_0.h_w_id;
    }
    const auto& h_w_id() const {
        return split_0.h_w_id;
    }
    auto& h_date() {
        return split_0.h_date;
    }
    const auto& h_date() const {
        return split_0.h_date;
    }
    auto& h_amount() {
        return split_1.h_amount;
    }
    const auto& h_amount() const {
        return split_1.h_amount;
    }
    auto& h_data() {
        return split_1.h_data;
    }
    const auto& h_data() const {
        return split_1.h_data;
    }

    split_value<0, 6> split_0;
    split_value<6, 8> split_1;
};

template <>
struct split_value<0, 7> {
    explicit split_value() = default;
    accessor<0> h_c_id;
    accessor<1> h_c_d_id;
    accessor<2> h_c_w_id;
    accessor<3> h_d_id;
    accessor<4> h_w_id;
    accessor<5> h_date;
    accessor<6> h_amount;
};
template <>
struct split_value<7, 8> {
    explicit split_value() = default;
    accessor<7> h_data;
};
template <>
struct unified_value<7> {
    auto& h_c_id() {
        return split_0.h_c_id;
    }
    const auto& h_c_id() const {
        return split_0.h_c_id;
    }
    auto& h_c_d_id() {
        return split_0.h_c_d_id;
    }
    const auto& h_c_d_id() const {
        return split_0.h_c_d_id;
    }
    auto& h_c_w_id() {
        return split_0.h_c_w_id;
    }
    const auto& h_c_w_id() const {
        return split_0.h_c_w_id;
    }
    auto& h_d_id() {
        return split_0.h_d_id;
    }
    const auto& h_d_id() const {
        return split_0.h_d_id;
    }
    auto& h_w_id() {
        return split_0.h_w_id;
    }
    const auto& h_w_id() const {
        return split_0.h_w_id;
    }
    auto& h_date() {
        return split_0.h_date;
    }
    const auto& h_date() const {
        return split_0.h_date;
    }
    auto& h_amount() {
        return split_0.h_amount;
    }
    const auto& h_amount() const {
        return split_0.h_amount;
    }
    auto& h_data() {
        return split_1.h_data;
    }
    const auto& h_data() const {
        return split_1.h_data;
    }

    split_value<0, 7> split_0;
    split_value<7, 8> split_1;
};

template <>
struct split_value<0, 8> {
    explicit split_value() = default;
    accessor<0> h_c_id;
    accessor<1> h_c_d_id;
    accessor<2> h_c_w_id;
    accessor<3> h_d_id;
    accessor<4> h_w_id;
    accessor<5> h_date;
    accessor<6> h_amount;
    accessor<7> h_data;
};
template <>
struct unified_value<8> {
    auto& h_c_id() {
        return split_0.h_c_id;
    }
    const auto& h_c_id() const {
        return split_0.h_c_id;
    }
    auto& h_c_d_id() {
        return split_0.h_c_d_id;
    }
    const auto& h_c_d_id() const {
        return split_0.h_c_d_id;
    }
    auto& h_c_w_id() {
        return split_0.h_c_w_id;
    }
    const auto& h_c_w_id() const {
        return split_0.h_c_w_id;
    }
    auto& h_d_id() {
        return split_0.h_d_id;
    }
    const auto& h_d_id() const {
        return split_0.h_d_id;
    }
    auto& h_w_id() {
        return split_0.h_w_id;
    }
    const auto& h_w_id() const {
        return split_0.h_w_id;
    }
    auto& h_date() {
        return split_0.h_date;
    }
    const auto& h_date() const {
        return split_0.h_date;
    }
    auto& h_amount() {
        return split_0.h_amount;
    }
    const auto& h_amount() const {
        return split_0.h_amount;
    }
    auto& h_data() {
        return split_0.h_data;
    }
    const auto& h_data() const {
        return split_0.h_data;
    }

    split_value<0, 8> split_0;
};

struct history_value {
    explicit history_value() = default;

    using NamedColumn = history_value_datatypes::NamedColumn;

    auto& h_c_id() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.h_c_id();
            }, value);
    }
    const auto& h_c_id() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.h_c_id();
            }, value);
    }
    auto& h_c_d_id() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.h_c_d_id();
            }, value);
    }
    const auto& h_c_d_id() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.h_c_d_id();
            }, value);
    }
    auto& h_c_w_id() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.h_c_w_id();
            }, value);
    }
    const auto& h_c_w_id() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.h_c_w_id();
            }, value);
    }
    auto& h_d_id() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.h_d_id();
            }, value);
    }
    const auto& h_d_id() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.h_d_id();
            }, value);
    }
    auto& h_w_id() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.h_w_id();
            }, value);
    }
    const auto& h_w_id() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.h_w_id();
            }, value);
    }
    auto& h_date() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.h_date();
            }, value);
    }
    const auto& h_date() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.h_date();
            }, value);
    }
    auto& h_amount() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.h_amount();
            }, value);
    }
    const auto& h_amount() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.h_amount();
            }, value);
    }
    auto& h_data() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.h_data();
            }, value);
    }
    const auto& h_data() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.h_data();
            }, value);
    }

    std::variant<
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
};  // namespace history_value_datatypes

using history_value = history_value_datatypes::history_value;
using ADAPTER_OF(history_value) = ADAPTER_OF(history_value_datatypes::history_value);

namespace order_value_datatypes {

enum class NamedColumn : int {
    o_c_id = 0,
    o_entry_d = 1,
    o_ol_cnt = 2,
    o_all_local = 3,
    o_carrier_id = 4,
    COLCOUNT = 5
};

template <size_t ColIndex>
struct accessor;

template <size_t StartIndex, size_t EndIndex>
struct split_value;

template <size_t SplitIndex>
struct unified_value;

struct order_value;

DEFINE_ADAPTER(order_value, 5);

template <size_t ColIndex>
struct accessor_info;

template <>
struct accessor_info<0> {
    using type = uint64_t;
};

template <>
struct accessor_info<1> {
    using type = uint32_t;
};

template <>
struct accessor_info<2> {
    using type = uint32_t;
};

template <>
struct accessor_info<3> {
    using type = uint32_t;
};

template <>
struct accessor_info<4> {
    using type = uint64_t;
};

template <size_t ColIndex>
struct accessor {
    using type = typename accessor_info<ColIndex>::type;

    accessor() = default;
    accessor(type& value) : value_(value) {}
    accessor(const type& value) : value_(const_cast<type&>(value)) {}

    operator type() {
        ADAPTER_OF(order_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    operator const type() const {
        ADAPTER_OF(order_value)::CountRead(ColIndex + index_);
        return value_;
    }

    operator type&() {
        ADAPTER_OF(order_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    operator const type&() const {
        ADAPTER_OF(order_value)::CountRead(ColIndex + index_);
        return value_;
    }

    type operator =(const type& other) {
        ADAPTER_OF(order_value)::CountWrite(ColIndex + index_);
        return value_ = other;
    }

    type operator =(const accessor<ColIndex>& other) {
        ADAPTER_OF(order_value)::CountWrite(ColIndex + index_);
        return value_ = other.value_;
    }

    type operator *() {
        ADAPTER_OF(order_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    const type operator *() const {
        ADAPTER_OF(order_value)::CountRead(ColIndex + index_);
        return value_;
    }

    type* operator ->() {
        ADAPTER_OF(order_value)::CountWrite(ColIndex + index_);
        return &value_;
    }

    const type* operator ->() const {
        ADAPTER_OF(order_value)::CountRead(ColIndex + index_);
        return &value_;
    }

    size_t index_ = 0;
    type value_;
};

template <>
struct split_value<0, 1> {
    explicit split_value() = default;
    accessor<0> o_c_id;
};
template <>
struct split_value<1, 5> {
    explicit split_value() = default;
    accessor<1> o_entry_d;
    accessor<2> o_ol_cnt;
    accessor<3> o_all_local;
    accessor<4> o_carrier_id;
};
template <>
struct unified_value<1> {
    auto& o_c_id() {
        return split_0.o_c_id;
    }
    const auto& o_c_id() const {
        return split_0.o_c_id;
    }
    auto& o_entry_d() {
        return split_1.o_entry_d;
    }
    const auto& o_entry_d() const {
        return split_1.o_entry_d;
    }
    auto& o_ol_cnt() {
        return split_1.o_ol_cnt;
    }
    const auto& o_ol_cnt() const {
        return split_1.o_ol_cnt;
    }
    auto& o_all_local() {
        return split_1.o_all_local;
    }
    const auto& o_all_local() const {
        return split_1.o_all_local;
    }
    auto& o_carrier_id() {
        return split_1.o_carrier_id;
    }
    const auto& o_carrier_id() const {
        return split_1.o_carrier_id;
    }

    split_value<0, 1> split_0;
    split_value<1, 5> split_1;
};

template <>
struct split_value<0, 2> {
    explicit split_value() = default;
    accessor<0> o_c_id;
    accessor<1> o_entry_d;
};
template <>
struct split_value<2, 5> {
    explicit split_value() = default;
    accessor<2> o_ol_cnt;
    accessor<3> o_all_local;
    accessor<4> o_carrier_id;
};
template <>
struct unified_value<2> {
    auto& o_c_id() {
        return split_0.o_c_id;
    }
    const auto& o_c_id() const {
        return split_0.o_c_id;
    }
    auto& o_entry_d() {
        return split_0.o_entry_d;
    }
    const auto& o_entry_d() const {
        return split_0.o_entry_d;
    }
    auto& o_ol_cnt() {
        return split_1.o_ol_cnt;
    }
    const auto& o_ol_cnt() const {
        return split_1.o_ol_cnt;
    }
    auto& o_all_local() {
        return split_1.o_all_local;
    }
    const auto& o_all_local() const {
        return split_1.o_all_local;
    }
    auto& o_carrier_id() {
        return split_1.o_carrier_id;
    }
    const auto& o_carrier_id() const {
        return split_1.o_carrier_id;
    }

    split_value<0, 2> split_0;
    split_value<2, 5> split_1;
};

template <>
struct split_value<0, 3> {
    explicit split_value() = default;
    accessor<0> o_c_id;
    accessor<1> o_entry_d;
    accessor<2> o_ol_cnt;
};
template <>
struct split_value<3, 5> {
    explicit split_value() = default;
    accessor<3> o_all_local;
    accessor<4> o_carrier_id;
};
template <>
struct unified_value<3> {
    auto& o_c_id() {
        return split_0.o_c_id;
    }
    const auto& o_c_id() const {
        return split_0.o_c_id;
    }
    auto& o_entry_d() {
        return split_0.o_entry_d;
    }
    const auto& o_entry_d() const {
        return split_0.o_entry_d;
    }
    auto& o_ol_cnt() {
        return split_0.o_ol_cnt;
    }
    const auto& o_ol_cnt() const {
        return split_0.o_ol_cnt;
    }
    auto& o_all_local() {
        return split_1.o_all_local;
    }
    const auto& o_all_local() const {
        return split_1.o_all_local;
    }
    auto& o_carrier_id() {
        return split_1.o_carrier_id;
    }
    const auto& o_carrier_id() const {
        return split_1.o_carrier_id;
    }

    split_value<0, 3> split_0;
    split_value<3, 5> split_1;
};

template <>
struct split_value<0, 4> {
    explicit split_value() = default;
    accessor<0> o_c_id;
    accessor<1> o_entry_d;
    accessor<2> o_ol_cnt;
    accessor<3> o_all_local;
};
template <>
struct split_value<4, 5> {
    explicit split_value() = default;
    accessor<4> o_carrier_id;
};
template <>
struct unified_value<4> {
    auto& o_c_id() {
        return split_0.o_c_id;
    }
    const auto& o_c_id() const {
        return split_0.o_c_id;
    }
    auto& o_entry_d() {
        return split_0.o_entry_d;
    }
    const auto& o_entry_d() const {
        return split_0.o_entry_d;
    }
    auto& o_ol_cnt() {
        return split_0.o_ol_cnt;
    }
    const auto& o_ol_cnt() const {
        return split_0.o_ol_cnt;
    }
    auto& o_all_local() {
        return split_0.o_all_local;
    }
    const auto& o_all_local() const {
        return split_0.o_all_local;
    }
    auto& o_carrier_id() {
        return split_1.o_carrier_id;
    }
    const auto& o_carrier_id() const {
        return split_1.o_carrier_id;
    }

    split_value<0, 4> split_0;
    split_value<4, 5> split_1;
};

template <>
struct split_value<0, 5> {
    explicit split_value() = default;
    accessor<0> o_c_id;
    accessor<1> o_entry_d;
    accessor<2> o_ol_cnt;
    accessor<3> o_all_local;
    accessor<4> o_carrier_id;
};
template <>
struct unified_value<5> {
    auto& o_c_id() {
        return split_0.o_c_id;
    }
    const auto& o_c_id() const {
        return split_0.o_c_id;
    }
    auto& o_entry_d() {
        return split_0.o_entry_d;
    }
    const auto& o_entry_d() const {
        return split_0.o_entry_d;
    }
    auto& o_ol_cnt() {
        return split_0.o_ol_cnt;
    }
    const auto& o_ol_cnt() const {
        return split_0.o_ol_cnt;
    }
    auto& o_all_local() {
        return split_0.o_all_local;
    }
    const auto& o_all_local() const {
        return split_0.o_all_local;
    }
    auto& o_carrier_id() {
        return split_0.o_carrier_id;
    }
    const auto& o_carrier_id() const {
        return split_0.o_carrier_id;
    }

    split_value<0, 5> split_0;
};

struct order_value {
    explicit order_value() = default;

    using NamedColumn = order_value_datatypes::NamedColumn;

    auto& o_c_id() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.o_c_id();
            }, value);
    }
    const auto& o_c_id() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.o_c_id();
            }, value);
    }
    auto& o_entry_d() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.o_entry_d();
            }, value);
    }
    const auto& o_entry_d() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.o_entry_d();
            }, value);
    }
    auto& o_ol_cnt() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.o_ol_cnt();
            }, value);
    }
    const auto& o_ol_cnt() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.o_ol_cnt();
            }, value);
    }
    auto& o_all_local() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.o_all_local();
            }, value);
    }
    const auto& o_all_local() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.o_all_local();
            }, value);
    }
    auto& o_carrier_id() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.o_carrier_id();
            }, value);
    }
    const auto& o_carrier_id() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.o_carrier_id();
            }, value);
    }

    std::variant<
        unified_value<5>,
        unified_value<4>,
        unified_value<3>,
        unified_value<2>,
        unified_value<1>
        > value;
};
};  // namespace order_value_datatypes

using order_value = order_value_datatypes::order_value;
using ADAPTER_OF(order_value) = ADAPTER_OF(order_value_datatypes::order_value);

namespace orderline_value_datatypes {

enum class NamedColumn : int {
    ol_i_id = 0,
    ol_supply_w_id = 1,
    ol_quantity = 2,
    ol_amount = 3,
    ol_dist_info = 4,
    ol_delivery_d = 5,
    COLCOUNT = 6
};

template <size_t ColIndex>
struct accessor;

template <size_t StartIndex, size_t EndIndex>
struct split_value;

template <size_t SplitIndex>
struct unified_value;

struct orderline_value;

DEFINE_ADAPTER(orderline_value, 6);

template <size_t ColIndex>
struct accessor_info;

template <>
struct accessor_info<0> {
    using type = uint64_t;
};

template <>
struct accessor_info<1> {
    using type = uint64_t;
};

template <>
struct accessor_info<2> {
    using type = uint32_t;
};

template <>
struct accessor_info<3> {
    using type = int32_t;
};

template <>
struct accessor_info<4> {
    using type = fix_string<24>;
};

template <>
struct accessor_info<5> {
    using type = uint32_t;
};

template <size_t ColIndex>
struct accessor {
    using type = typename accessor_info<ColIndex>::type;

    accessor() = default;
    accessor(type& value) : value_(value) {}
    accessor(const type& value) : value_(const_cast<type&>(value)) {}

    operator type() {
        ADAPTER_OF(orderline_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    operator const type() const {
        ADAPTER_OF(orderline_value)::CountRead(ColIndex + index_);
        return value_;
    }

    operator type&() {
        ADAPTER_OF(orderline_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    operator const type&() const {
        ADAPTER_OF(orderline_value)::CountRead(ColIndex + index_);
        return value_;
    }

    type operator =(const type& other) {
        ADAPTER_OF(orderline_value)::CountWrite(ColIndex + index_);
        return value_ = other;
    }

    type operator =(const accessor<ColIndex>& other) {
        ADAPTER_OF(orderline_value)::CountWrite(ColIndex + index_);
        return value_ = other.value_;
    }

    type operator *() {
        ADAPTER_OF(orderline_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    const type operator *() const {
        ADAPTER_OF(orderline_value)::CountRead(ColIndex + index_);
        return value_;
    }

    type* operator ->() {
        ADAPTER_OF(orderline_value)::CountWrite(ColIndex + index_);
        return &value_;
    }

    const type* operator ->() const {
        ADAPTER_OF(orderline_value)::CountRead(ColIndex + index_);
        return &value_;
    }

    size_t index_ = 0;
    type value_;
};

template <>
struct split_value<0, 1> {
    explicit split_value() = default;
    accessor<0> ol_i_id;
};
template <>
struct split_value<1, 6> {
    explicit split_value() = default;
    accessor<1> ol_supply_w_id;
    accessor<2> ol_quantity;
    accessor<3> ol_amount;
    accessor<4> ol_dist_info;
    accessor<5> ol_delivery_d;
};
template <>
struct unified_value<1> {
    auto& ol_i_id() {
        return split_0.ol_i_id;
    }
    const auto& ol_i_id() const {
        return split_0.ol_i_id;
    }
    auto& ol_supply_w_id() {
        return split_1.ol_supply_w_id;
    }
    const auto& ol_supply_w_id() const {
        return split_1.ol_supply_w_id;
    }
    auto& ol_quantity() {
        return split_1.ol_quantity;
    }
    const auto& ol_quantity() const {
        return split_1.ol_quantity;
    }
    auto& ol_amount() {
        return split_1.ol_amount;
    }
    const auto& ol_amount() const {
        return split_1.ol_amount;
    }
    auto& ol_dist_info() {
        return split_1.ol_dist_info;
    }
    const auto& ol_dist_info() const {
        return split_1.ol_dist_info;
    }
    auto& ol_delivery_d() {
        return split_1.ol_delivery_d;
    }
    const auto& ol_delivery_d() const {
        return split_1.ol_delivery_d;
    }

    split_value<0, 1> split_0;
    split_value<1, 6> split_1;
};

template <>
struct split_value<0, 2> {
    explicit split_value() = default;
    accessor<0> ol_i_id;
    accessor<1> ol_supply_w_id;
};
template <>
struct split_value<2, 6> {
    explicit split_value() = default;
    accessor<2> ol_quantity;
    accessor<3> ol_amount;
    accessor<4> ol_dist_info;
    accessor<5> ol_delivery_d;
};
template <>
struct unified_value<2> {
    auto& ol_i_id() {
        return split_0.ol_i_id;
    }
    const auto& ol_i_id() const {
        return split_0.ol_i_id;
    }
    auto& ol_supply_w_id() {
        return split_0.ol_supply_w_id;
    }
    const auto& ol_supply_w_id() const {
        return split_0.ol_supply_w_id;
    }
    auto& ol_quantity() {
        return split_1.ol_quantity;
    }
    const auto& ol_quantity() const {
        return split_1.ol_quantity;
    }
    auto& ol_amount() {
        return split_1.ol_amount;
    }
    const auto& ol_amount() const {
        return split_1.ol_amount;
    }
    auto& ol_dist_info() {
        return split_1.ol_dist_info;
    }
    const auto& ol_dist_info() const {
        return split_1.ol_dist_info;
    }
    auto& ol_delivery_d() {
        return split_1.ol_delivery_d;
    }
    const auto& ol_delivery_d() const {
        return split_1.ol_delivery_d;
    }

    split_value<0, 2> split_0;
    split_value<2, 6> split_1;
};

template <>
struct split_value<0, 3> {
    explicit split_value() = default;
    accessor<0> ol_i_id;
    accessor<1> ol_supply_w_id;
    accessor<2> ol_quantity;
};
template <>
struct split_value<3, 6> {
    explicit split_value() = default;
    accessor<3> ol_amount;
    accessor<4> ol_dist_info;
    accessor<5> ol_delivery_d;
};
template <>
struct unified_value<3> {
    auto& ol_i_id() {
        return split_0.ol_i_id;
    }
    const auto& ol_i_id() const {
        return split_0.ol_i_id;
    }
    auto& ol_supply_w_id() {
        return split_0.ol_supply_w_id;
    }
    const auto& ol_supply_w_id() const {
        return split_0.ol_supply_w_id;
    }
    auto& ol_quantity() {
        return split_0.ol_quantity;
    }
    const auto& ol_quantity() const {
        return split_0.ol_quantity;
    }
    auto& ol_amount() {
        return split_1.ol_amount;
    }
    const auto& ol_amount() const {
        return split_1.ol_amount;
    }
    auto& ol_dist_info() {
        return split_1.ol_dist_info;
    }
    const auto& ol_dist_info() const {
        return split_1.ol_dist_info;
    }
    auto& ol_delivery_d() {
        return split_1.ol_delivery_d;
    }
    const auto& ol_delivery_d() const {
        return split_1.ol_delivery_d;
    }

    split_value<0, 3> split_0;
    split_value<3, 6> split_1;
};

template <>
struct split_value<0, 4> {
    explicit split_value() = default;
    accessor<0> ol_i_id;
    accessor<1> ol_supply_w_id;
    accessor<2> ol_quantity;
    accessor<3> ol_amount;
};
template <>
struct split_value<4, 6> {
    explicit split_value() = default;
    accessor<4> ol_dist_info;
    accessor<5> ol_delivery_d;
};
template <>
struct unified_value<4> {
    auto& ol_i_id() {
        return split_0.ol_i_id;
    }
    const auto& ol_i_id() const {
        return split_0.ol_i_id;
    }
    auto& ol_supply_w_id() {
        return split_0.ol_supply_w_id;
    }
    const auto& ol_supply_w_id() const {
        return split_0.ol_supply_w_id;
    }
    auto& ol_quantity() {
        return split_0.ol_quantity;
    }
    const auto& ol_quantity() const {
        return split_0.ol_quantity;
    }
    auto& ol_amount() {
        return split_0.ol_amount;
    }
    const auto& ol_amount() const {
        return split_0.ol_amount;
    }
    auto& ol_dist_info() {
        return split_1.ol_dist_info;
    }
    const auto& ol_dist_info() const {
        return split_1.ol_dist_info;
    }
    auto& ol_delivery_d() {
        return split_1.ol_delivery_d;
    }
    const auto& ol_delivery_d() const {
        return split_1.ol_delivery_d;
    }

    split_value<0, 4> split_0;
    split_value<4, 6> split_1;
};

template <>
struct split_value<0, 5> {
    explicit split_value() = default;
    accessor<0> ol_i_id;
    accessor<1> ol_supply_w_id;
    accessor<2> ol_quantity;
    accessor<3> ol_amount;
    accessor<4> ol_dist_info;
};
template <>
struct split_value<5, 6> {
    explicit split_value() = default;
    accessor<5> ol_delivery_d;
};
template <>
struct unified_value<5> {
    auto& ol_i_id() {
        return split_0.ol_i_id;
    }
    const auto& ol_i_id() const {
        return split_0.ol_i_id;
    }
    auto& ol_supply_w_id() {
        return split_0.ol_supply_w_id;
    }
    const auto& ol_supply_w_id() const {
        return split_0.ol_supply_w_id;
    }
    auto& ol_quantity() {
        return split_0.ol_quantity;
    }
    const auto& ol_quantity() const {
        return split_0.ol_quantity;
    }
    auto& ol_amount() {
        return split_0.ol_amount;
    }
    const auto& ol_amount() const {
        return split_0.ol_amount;
    }
    auto& ol_dist_info() {
        return split_0.ol_dist_info;
    }
    const auto& ol_dist_info() const {
        return split_0.ol_dist_info;
    }
    auto& ol_delivery_d() {
        return split_1.ol_delivery_d;
    }
    const auto& ol_delivery_d() const {
        return split_1.ol_delivery_d;
    }

    split_value<0, 5> split_0;
    split_value<5, 6> split_1;
};

template <>
struct split_value<0, 6> {
    explicit split_value() = default;
    accessor<0> ol_i_id;
    accessor<1> ol_supply_w_id;
    accessor<2> ol_quantity;
    accessor<3> ol_amount;
    accessor<4> ol_dist_info;
    accessor<5> ol_delivery_d;
};
template <>
struct unified_value<6> {
    auto& ol_i_id() {
        return split_0.ol_i_id;
    }
    const auto& ol_i_id() const {
        return split_0.ol_i_id;
    }
    auto& ol_supply_w_id() {
        return split_0.ol_supply_w_id;
    }
    const auto& ol_supply_w_id() const {
        return split_0.ol_supply_w_id;
    }
    auto& ol_quantity() {
        return split_0.ol_quantity;
    }
    const auto& ol_quantity() const {
        return split_0.ol_quantity;
    }
    auto& ol_amount() {
        return split_0.ol_amount;
    }
    const auto& ol_amount() const {
        return split_0.ol_amount;
    }
    auto& ol_dist_info() {
        return split_0.ol_dist_info;
    }
    const auto& ol_dist_info() const {
        return split_0.ol_dist_info;
    }
    auto& ol_delivery_d() {
        return split_0.ol_delivery_d;
    }
    const auto& ol_delivery_d() const {
        return split_0.ol_delivery_d;
    }

    split_value<0, 6> split_0;
};

struct orderline_value {
    explicit orderline_value() = default;

    using NamedColumn = orderline_value_datatypes::NamedColumn;

    auto& ol_i_id() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.ol_i_id();
            }, value);
    }
    const auto& ol_i_id() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.ol_i_id();
            }, value);
    }
    auto& ol_supply_w_id() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.ol_supply_w_id();
            }, value);
    }
    const auto& ol_supply_w_id() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.ol_supply_w_id();
            }, value);
    }
    auto& ol_quantity() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.ol_quantity();
            }, value);
    }
    const auto& ol_quantity() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.ol_quantity();
            }, value);
    }
    auto& ol_amount() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.ol_amount();
            }, value);
    }
    const auto& ol_amount() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.ol_amount();
            }, value);
    }
    auto& ol_dist_info() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.ol_dist_info();
            }, value);
    }
    const auto& ol_dist_info() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.ol_dist_info();
            }, value);
    }
    auto& ol_delivery_d() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.ol_delivery_d();
            }, value);
    }
    const auto& ol_delivery_d() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.ol_delivery_d();
            }, value);
    }

    std::variant<
        unified_value<6>,
        unified_value<5>,
        unified_value<4>,
        unified_value<3>,
        unified_value<2>,
        unified_value<1>
        > value;
};
};  // namespace orderline_value_datatypes

using orderline_value = orderline_value_datatypes::orderline_value;
using ADAPTER_OF(orderline_value) = ADAPTER_OF(orderline_value_datatypes::orderline_value);

namespace item_value_datatypes {

enum class NamedColumn : int {
    i_im_id = 0,
    i_price = 1,
    i_name = 2,
    i_data = 3,
    COLCOUNT = 4
};

template <size_t ColIndex>
struct accessor;

template <size_t StartIndex, size_t EndIndex>
struct split_value;

template <size_t SplitIndex>
struct unified_value;

struct item_value;

DEFINE_ADAPTER(item_value, 4);

template <size_t ColIndex>
struct accessor_info;

template <>
struct accessor_info<0> {
    using type = uint64_t;
};

template <>
struct accessor_info<1> {
    using type = uint32_t;
};

template <>
struct accessor_info<2> {
    using type = var_string<24>;
};

template <>
struct accessor_info<3> {
    using type = var_string<50>;
};

template <size_t ColIndex>
struct accessor {
    using type = typename accessor_info<ColIndex>::type;

    accessor() = default;
    accessor(type& value) : value_(value) {}
    accessor(const type& value) : value_(const_cast<type&>(value)) {}

    operator type() {
        ADAPTER_OF(item_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    operator const type() const {
        ADAPTER_OF(item_value)::CountRead(ColIndex + index_);
        return value_;
    }

    operator type&() {
        ADAPTER_OF(item_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    operator const type&() const {
        ADAPTER_OF(item_value)::CountRead(ColIndex + index_);
        return value_;
    }

    type operator =(const type& other) {
        ADAPTER_OF(item_value)::CountWrite(ColIndex + index_);
        return value_ = other;
    }

    type operator =(const accessor<ColIndex>& other) {
        ADAPTER_OF(item_value)::CountWrite(ColIndex + index_);
        return value_ = other.value_;
    }

    type operator *() {
        ADAPTER_OF(item_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    const type operator *() const {
        ADAPTER_OF(item_value)::CountRead(ColIndex + index_);
        return value_;
    }

    type* operator ->() {
        ADAPTER_OF(item_value)::CountWrite(ColIndex + index_);
        return &value_;
    }

    const type* operator ->() const {
        ADAPTER_OF(item_value)::CountRead(ColIndex + index_);
        return &value_;
    }

    size_t index_ = 0;
    type value_;
};

template <>
struct split_value<0, 1> {
    explicit split_value() = default;
    accessor<0> i_im_id;
};
template <>
struct split_value<1, 4> {
    explicit split_value() = default;
    accessor<1> i_price;
    accessor<2> i_name;
    accessor<3> i_data;
};
template <>
struct unified_value<1> {
    auto& i_im_id() {
        return split_0.i_im_id;
    }
    const auto& i_im_id() const {
        return split_0.i_im_id;
    }
    auto& i_price() {
        return split_1.i_price;
    }
    const auto& i_price() const {
        return split_1.i_price;
    }
    auto& i_name() {
        return split_1.i_name;
    }
    const auto& i_name() const {
        return split_1.i_name;
    }
    auto& i_data() {
        return split_1.i_data;
    }
    const auto& i_data() const {
        return split_1.i_data;
    }

    split_value<0, 1> split_0;
    split_value<1, 4> split_1;
};

template <>
struct split_value<0, 2> {
    explicit split_value() = default;
    accessor<0> i_im_id;
    accessor<1> i_price;
};
template <>
struct split_value<2, 4> {
    explicit split_value() = default;
    accessor<2> i_name;
    accessor<3> i_data;
};
template <>
struct unified_value<2> {
    auto& i_im_id() {
        return split_0.i_im_id;
    }
    const auto& i_im_id() const {
        return split_0.i_im_id;
    }
    auto& i_price() {
        return split_0.i_price;
    }
    const auto& i_price() const {
        return split_0.i_price;
    }
    auto& i_name() {
        return split_1.i_name;
    }
    const auto& i_name() const {
        return split_1.i_name;
    }
    auto& i_data() {
        return split_1.i_data;
    }
    const auto& i_data() const {
        return split_1.i_data;
    }

    split_value<0, 2> split_0;
    split_value<2, 4> split_1;
};

template <>
struct split_value<0, 3> {
    explicit split_value() = default;
    accessor<0> i_im_id;
    accessor<1> i_price;
    accessor<2> i_name;
};
template <>
struct split_value<3, 4> {
    explicit split_value() = default;
    accessor<3> i_data;
};
template <>
struct unified_value<3> {
    auto& i_im_id() {
        return split_0.i_im_id;
    }
    const auto& i_im_id() const {
        return split_0.i_im_id;
    }
    auto& i_price() {
        return split_0.i_price;
    }
    const auto& i_price() const {
        return split_0.i_price;
    }
    auto& i_name() {
        return split_0.i_name;
    }
    const auto& i_name() const {
        return split_0.i_name;
    }
    auto& i_data() {
        return split_1.i_data;
    }
    const auto& i_data() const {
        return split_1.i_data;
    }

    split_value<0, 3> split_0;
    split_value<3, 4> split_1;
};

template <>
struct split_value<0, 4> {
    explicit split_value() = default;
    accessor<0> i_im_id;
    accessor<1> i_price;
    accessor<2> i_name;
    accessor<3> i_data;
};
template <>
struct unified_value<4> {
    auto& i_im_id() {
        return split_0.i_im_id;
    }
    const auto& i_im_id() const {
        return split_0.i_im_id;
    }
    auto& i_price() {
        return split_0.i_price;
    }
    const auto& i_price() const {
        return split_0.i_price;
    }
    auto& i_name() {
        return split_0.i_name;
    }
    const auto& i_name() const {
        return split_0.i_name;
    }
    auto& i_data() {
        return split_0.i_data;
    }
    const auto& i_data() const {
        return split_0.i_data;
    }

    split_value<0, 4> split_0;
};

struct item_value {
    explicit item_value() = default;

    using NamedColumn = item_value_datatypes::NamedColumn;

    auto& i_im_id() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.i_im_id();
            }, value);
    }
    const auto& i_im_id() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.i_im_id();
            }, value);
    }
    auto& i_price() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.i_price();
            }, value);
    }
    const auto& i_price() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.i_price();
            }, value);
    }
    auto& i_name() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.i_name();
            }, value);
    }
    const auto& i_name() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.i_name();
            }, value);
    }
    auto& i_data() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.i_data();
            }, value);
    }
    const auto& i_data() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.i_data();
            }, value);
    }

    std::variant<
        unified_value<4>,
        unified_value<3>,
        unified_value<2>,
        unified_value<1>
        > value;
};
};  // namespace item_value_datatypes

using item_value = item_value_datatypes::item_value;
using ADAPTER_OF(item_value) = ADAPTER_OF(item_value_datatypes::item_value);

namespace stock_value_datatypes {

enum class NamedColumn : int {
    s_dists = 0,
    s_data = 1,
    s_quantity = 2,
    s_ytd = 3,
    s_order_cnt = 4,
    s_remote_cnt = 5,
    COLCOUNT = 6
};

template <size_t ColIndex>
struct accessor;

template <size_t StartIndex, size_t EndIndex>
struct split_value;

template <size_t SplitIndex>
struct unified_value;

struct stock_value;

DEFINE_ADAPTER(stock_value, 6);

template <size_t ColIndex>
struct accessor_info;

template <>
struct accessor_info<0> {
    using type = std::array<fix_string<24>, NUM_DISTRICTS_PER_WAREHOUSE>;
};

template <>
struct accessor_info<1> {
    using type = var_string<50>;
};

template <>
struct accessor_info<2> {
    using type = int32_t;
};

template <>
struct accessor_info<3> {
    using type = uint32_t;
};

template <>
struct accessor_info<4> {
    using type = uint32_t;
};

template <>
struct accessor_info<5> {
    using type = uint32_t;
};

template <size_t ColIndex>
struct accessor {
    using type = typename accessor_info<ColIndex>::type;

    accessor() = default;
    accessor(type& value) : value_(value) {}
    accessor(const type& value) : value_(const_cast<type&>(value)) {}

    operator type() {
        ADAPTER_OF(stock_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    operator const type() const {
        ADAPTER_OF(stock_value)::CountRead(ColIndex + index_);
        return value_;
    }

    operator type&() {
        ADAPTER_OF(stock_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    operator const type&() const {
        ADAPTER_OF(stock_value)::CountRead(ColIndex + index_);
        return value_;
    }

    type operator =(const type& other) {
        ADAPTER_OF(stock_value)::CountWrite(ColIndex + index_);
        return value_ = other;
    }

    type operator =(const accessor<ColIndex>& other) {
        ADAPTER_OF(stock_value)::CountWrite(ColIndex + index_);
        return value_ = other.value_;
    }

    type operator *() {
        ADAPTER_OF(stock_value)::CountWrite(ColIndex + index_);
        return value_;
    }

    const type operator *() const {
        ADAPTER_OF(stock_value)::CountRead(ColIndex + index_);
        return value_;
    }

    type* operator ->() {
        ADAPTER_OF(stock_value)::CountWrite(ColIndex + index_);
        return &value_;
    }

    const type* operator ->() const {
        ADAPTER_OF(stock_value)::CountRead(ColIndex + index_);
        return &value_;
    }

    size_t index_ = 0;
    type value_;
};

template <>
struct split_value<0, 1> {
    explicit split_value() = default;
    accessor<0> s_dists;
};
template <>
struct split_value<1, 6> {
    explicit split_value() = default;
    accessor<1> s_data;
    accessor<2> s_quantity;
    accessor<3> s_ytd;
    accessor<4> s_order_cnt;
    accessor<5> s_remote_cnt;
};
template <>
struct unified_value<1> {
    auto& s_dists() {
        return split_0.s_dists;
    }
    const auto& s_dists() const {
        return split_0.s_dists;
    }
    auto& s_data() {
        return split_1.s_data;
    }
    const auto& s_data() const {
        return split_1.s_data;
    }
    auto& s_quantity() {
        return split_1.s_quantity;
    }
    const auto& s_quantity() const {
        return split_1.s_quantity;
    }
    auto& s_ytd() {
        return split_1.s_ytd;
    }
    const auto& s_ytd() const {
        return split_1.s_ytd;
    }
    auto& s_order_cnt() {
        return split_1.s_order_cnt;
    }
    const auto& s_order_cnt() const {
        return split_1.s_order_cnt;
    }
    auto& s_remote_cnt() {
        return split_1.s_remote_cnt;
    }
    const auto& s_remote_cnt() const {
        return split_1.s_remote_cnt;
    }

    split_value<0, 1> split_0;
    split_value<1, 6> split_1;
};

template <>
struct split_value<0, 2> {
    explicit split_value() = default;
    accessor<0> s_dists;
    accessor<1> s_data;
};
template <>
struct split_value<2, 6> {
    explicit split_value() = default;
    accessor<2> s_quantity;
    accessor<3> s_ytd;
    accessor<4> s_order_cnt;
    accessor<5> s_remote_cnt;
};
template <>
struct unified_value<2> {
    auto& s_dists() {
        return split_0.s_dists;
    }
    const auto& s_dists() const {
        return split_0.s_dists;
    }
    auto& s_data() {
        return split_0.s_data;
    }
    const auto& s_data() const {
        return split_0.s_data;
    }
    auto& s_quantity() {
        return split_1.s_quantity;
    }
    const auto& s_quantity() const {
        return split_1.s_quantity;
    }
    auto& s_ytd() {
        return split_1.s_ytd;
    }
    const auto& s_ytd() const {
        return split_1.s_ytd;
    }
    auto& s_order_cnt() {
        return split_1.s_order_cnt;
    }
    const auto& s_order_cnt() const {
        return split_1.s_order_cnt;
    }
    auto& s_remote_cnt() {
        return split_1.s_remote_cnt;
    }
    const auto& s_remote_cnt() const {
        return split_1.s_remote_cnt;
    }

    split_value<0, 2> split_0;
    split_value<2, 6> split_1;
};

template <>
struct split_value<0, 3> {
    explicit split_value() = default;
    accessor<0> s_dists;
    accessor<1> s_data;
    accessor<2> s_quantity;
};
template <>
struct split_value<3, 6> {
    explicit split_value() = default;
    accessor<3> s_ytd;
    accessor<4> s_order_cnt;
    accessor<5> s_remote_cnt;
};
template <>
struct unified_value<3> {
    auto& s_dists() {
        return split_0.s_dists;
    }
    const auto& s_dists() const {
        return split_0.s_dists;
    }
    auto& s_data() {
        return split_0.s_data;
    }
    const auto& s_data() const {
        return split_0.s_data;
    }
    auto& s_quantity() {
        return split_0.s_quantity;
    }
    const auto& s_quantity() const {
        return split_0.s_quantity;
    }
    auto& s_ytd() {
        return split_1.s_ytd;
    }
    const auto& s_ytd() const {
        return split_1.s_ytd;
    }
    auto& s_order_cnt() {
        return split_1.s_order_cnt;
    }
    const auto& s_order_cnt() const {
        return split_1.s_order_cnt;
    }
    auto& s_remote_cnt() {
        return split_1.s_remote_cnt;
    }
    const auto& s_remote_cnt() const {
        return split_1.s_remote_cnt;
    }

    split_value<0, 3> split_0;
    split_value<3, 6> split_1;
};

template <>
struct split_value<0, 4> {
    explicit split_value() = default;
    accessor<0> s_dists;
    accessor<1> s_data;
    accessor<2> s_quantity;
    accessor<3> s_ytd;
};
template <>
struct split_value<4, 6> {
    explicit split_value() = default;
    accessor<4> s_order_cnt;
    accessor<5> s_remote_cnt;
};
template <>
struct unified_value<4> {
    auto& s_dists() {
        return split_0.s_dists;
    }
    const auto& s_dists() const {
        return split_0.s_dists;
    }
    auto& s_data() {
        return split_0.s_data;
    }
    const auto& s_data() const {
        return split_0.s_data;
    }
    auto& s_quantity() {
        return split_0.s_quantity;
    }
    const auto& s_quantity() const {
        return split_0.s_quantity;
    }
    auto& s_ytd() {
        return split_0.s_ytd;
    }
    const auto& s_ytd() const {
        return split_0.s_ytd;
    }
    auto& s_order_cnt() {
        return split_1.s_order_cnt;
    }
    const auto& s_order_cnt() const {
        return split_1.s_order_cnt;
    }
    auto& s_remote_cnt() {
        return split_1.s_remote_cnt;
    }
    const auto& s_remote_cnt() const {
        return split_1.s_remote_cnt;
    }

    split_value<0, 4> split_0;
    split_value<4, 6> split_1;
};

template <>
struct split_value<0, 5> {
    explicit split_value() = default;
    accessor<0> s_dists;
    accessor<1> s_data;
    accessor<2> s_quantity;
    accessor<3> s_ytd;
    accessor<4> s_order_cnt;
};
template <>
struct split_value<5, 6> {
    explicit split_value() = default;
    accessor<5> s_remote_cnt;
};
template <>
struct unified_value<5> {
    auto& s_dists() {
        return split_0.s_dists;
    }
    const auto& s_dists() const {
        return split_0.s_dists;
    }
    auto& s_data() {
        return split_0.s_data;
    }
    const auto& s_data() const {
        return split_0.s_data;
    }
    auto& s_quantity() {
        return split_0.s_quantity;
    }
    const auto& s_quantity() const {
        return split_0.s_quantity;
    }
    auto& s_ytd() {
        return split_0.s_ytd;
    }
    const auto& s_ytd() const {
        return split_0.s_ytd;
    }
    auto& s_order_cnt() {
        return split_0.s_order_cnt;
    }
    const auto& s_order_cnt() const {
        return split_0.s_order_cnt;
    }
    auto& s_remote_cnt() {
        return split_1.s_remote_cnt;
    }
    const auto& s_remote_cnt() const {
        return split_1.s_remote_cnt;
    }

    split_value<0, 5> split_0;
    split_value<5, 6> split_1;
};

template <>
struct split_value<0, 6> {
    explicit split_value() = default;
    accessor<0> s_dists;
    accessor<1> s_data;
    accessor<2> s_quantity;
    accessor<3> s_ytd;
    accessor<4> s_order_cnt;
    accessor<5> s_remote_cnt;
};
template <>
struct unified_value<6> {
    auto& s_dists() {
        return split_0.s_dists;
    }
    const auto& s_dists() const {
        return split_0.s_dists;
    }
    auto& s_data() {
        return split_0.s_data;
    }
    const auto& s_data() const {
        return split_0.s_data;
    }
    auto& s_quantity() {
        return split_0.s_quantity;
    }
    const auto& s_quantity() const {
        return split_0.s_quantity;
    }
    auto& s_ytd() {
        return split_0.s_ytd;
    }
    const auto& s_ytd() const {
        return split_0.s_ytd;
    }
    auto& s_order_cnt() {
        return split_0.s_order_cnt;
    }
    const auto& s_order_cnt() const {
        return split_0.s_order_cnt;
    }
    auto& s_remote_cnt() {
        return split_0.s_remote_cnt;
    }
    const auto& s_remote_cnt() const {
        return split_0.s_remote_cnt;
    }

    split_value<0, 6> split_0;
};

struct stock_value {
    explicit stock_value() = default;

    using NamedColumn = stock_value_datatypes::NamedColumn;

    auto& s_dists() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.s_dists();
            }, value);
    }
    const auto& s_dists() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.s_dists();
            }, value);
    }
    auto& s_data() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.s_data();
            }, value);
    }
    const auto& s_data() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.s_data();
            }, value);
    }
    auto& s_quantity() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.s_quantity();
            }, value);
    }
    const auto& s_quantity() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.s_quantity();
            }, value);
    }
    auto& s_ytd() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.s_ytd();
            }, value);
    }
    const auto& s_ytd() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.s_ytd();
            }, value);
    }
    auto& s_order_cnt() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.s_order_cnt();
            }, value);
    }
    const auto& s_order_cnt() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.s_order_cnt();
            }, value);
    }
    auto& s_remote_cnt() {
        return std::visit([this] (auto&& val) -> auto& {
                return val.s_remote_cnt();
            }, value);
    }
    const auto& s_remote_cnt() const {
        return std::visit([this] (auto&& val) -> const auto& {
                return val.s_remote_cnt();
            }, value);
    }

    std::variant<
        unified_value<6>,
        unified_value<5>,
        unified_value<4>,
        unified_value<3>,
        unified_value<2>,
        unified_value<1>
        > value;
};
};  // namespace stock_value_datatypes

using stock_value = stock_value_datatypes::stock_value;
using ADAPTER_OF(stock_value) = ADAPTER_OF(stock_value_datatypes::stock_value);

