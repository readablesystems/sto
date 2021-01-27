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

CREATE_ADAPTER(ycsb_value, 2);

template <>
struct accessor<0> {
    using type = fix_string<COL_WIDTH>;
    using access_type = std::array<accessor<0>, HALF_NUM_COLUMNS>;

    accessor() = default;
    accessor(type& value) : value_(value) {}
    accessor(const type& value) : value_((type)value) {}

    operator type() {
        ADAPTER_OF(ycsb_value)::CountRead(0);
        return value_;
    }
    operator const type() const {
        ADAPTER_OF(ycsb_value)::CountRead(0);
        return value_;
    }
    operator type&() {
        ADAPTER_OF(ycsb_value)::CountRead(0);
        return value_;
    }
    operator const type&() const {
        ADAPTER_OF(ycsb_value)::CountRead(0);
        return value_;
    }
    type operator =(const type& other) {
        ADAPTER_OF(ycsb_value)::CountWrite(0);
        return value_ = other;
    }
    type operator =(const accessor<0>& other) {
        ADAPTER_OF(ycsb_value)::CountWrite(0);
        return value_ = other.value_;
    }
    type operator *() {
        ADAPTER_OF(ycsb_value)::CountRead(0);
        return value_;
    }
    const type operator *() const {
        ADAPTER_OF(ycsb_value)::CountRead(0);
        return value_;
    }
    type* operator ->() {
        ADAPTER_OF(ycsb_value)::CountRead(0);
        return &value_;
    }
    const type* operator ->() const {
        ADAPTER_OF(ycsb_value)::CountRead(0);
        return &value_;
    }

    type value_;
};
template <>
struct accessor<1> {
    using type = fix_string<COL_WIDTH>;
    using access_type = std::array<accessor<1>, HALF_NUM_COLUMNS>;

    accessor() = default;
    accessor(type& value) : value_(value) {}
    accessor(const type& value) : value_((type)value) {}

    operator type() {
        ADAPTER_OF(ycsb_value)::CountRead(1);
        return value_;
    }
    operator const type() const {
        ADAPTER_OF(ycsb_value)::CountRead(1);
        return value_;
    }
    operator type&() {
        ADAPTER_OF(ycsb_value)::CountRead(1);
        return value_;
    }
    operator const type&() const {
        ADAPTER_OF(ycsb_value)::CountRead(1);
        return value_;
    }
    type operator =(const type& other) {
        ADAPTER_OF(ycsb_value)::CountWrite(1);
        return value_ = other;
    }
    type operator =(const accessor<1>& other) {
        ADAPTER_OF(ycsb_value)::CountWrite(1);
        return value_ = other.value_;
    }
    type operator *() {
        ADAPTER_OF(ycsb_value)::CountRead(1);
        return value_;
    }
    const type operator *() const {
        ADAPTER_OF(ycsb_value)::CountRead(1);
        return value_;
    }
    type* operator ->() {
        ADAPTER_OF(ycsb_value)::CountRead(1);
        return &value_;
    }
    const type* operator ->() const {
        ADAPTER_OF(ycsb_value)::CountRead(1);
        return &value_;
    }

    type value_;
};

template <>
struct split_value<0, 1> {
    explicit split_value() = default;
    std::array<accessor<0>, HALF_NUM_COLUMNS> even_columns;
};
template <>
struct split_value<1, 2> {
    explicit split_value() = default;
    std::array<accessor<1>, HALF_NUM_COLUMNS> odd_columns;
};
template <>
struct unified_value<1> {
    std::array<accessor<0>, HALF_NUM_COLUMNS>& even_columns() {
        return split_0.even_columns;
    }
    const std::array<accessor<0>, HALF_NUM_COLUMNS>& even_columns() const {
        return split_0.even_columns;
    }
    std::array<accessor<1>, HALF_NUM_COLUMNS>& odd_columns() {
        return split_1.odd_columns;
    }
    const std::array<accessor<1>, HALF_NUM_COLUMNS>& odd_columns() const {
        return split_1.odd_columns;
    }

    split_value<0, 1> split_0;
    split_value<1, 2> split_1;
};

template <>
struct split_value<0, 2> {
    explicit split_value() = default;
    std::array<accessor<0>, HALF_NUM_COLUMNS> even_columns;
    std::array<accessor<1>, HALF_NUM_COLUMNS> odd_columns;
};
template <>
struct unified_value<2> {
    std::array<accessor<0>, HALF_NUM_COLUMNS>& even_columns() {
        return split_0.even_columns;
    }
    const std::array<accessor<0>, HALF_NUM_COLUMNS>& even_columns() const {
        return split_0.even_columns;
    }
    std::array<accessor<1>, HALF_NUM_COLUMNS>& odd_columns() {
        return split_0.odd_columns;
    }
    const std::array<accessor<1>, HALF_NUM_COLUMNS>& odd_columns() const {
        return split_0.odd_columns;
    }

    split_value<0, 2> split_0;
};

struct ycsb_value {
    explicit ycsb_value() = default;

    using NamedColumn = ycsb_value_datatypes::NamedColumn;

    template <size_t Index>
    inline typename accessor<Index>::access_type& get();
    template <size_t Index>
    inline const typename accessor<Index>::access_type& get() const;

    std::array<accessor<0>, HALF_NUM_COLUMNS>& even_columns() {
        if (auto val = std::get_if<unified_value<1>>(&value)) {
            return val->even_columns();
        }
        return std::get<unified_value<2>>(value).even_columns();
    }
    const std::array<accessor<0>, HALF_NUM_COLUMNS>& even_columns() const {
        if (auto val = std::get_if<unified_value<1>>(&value)) {
            return val->even_columns();
        }
        return std::get<unified_value<2>>(value).even_columns();
    }
    std::array<accessor<1>, HALF_NUM_COLUMNS>& odd_columns() {
        if (auto val = std::get_if<unified_value<1>>(&value)) {
            return val->odd_columns();
        }
        return std::get<unified_value<2>>(value).odd_columns();
    }
    const std::array<accessor<1>, HALF_NUM_COLUMNS>& odd_columns() const {
        if (auto val = std::get_if<unified_value<1>>(&value)) {
            return val->odd_columns();
        }
        return std::get<unified_value<2>>(value).odd_columns();
    }

    std::variant<
        unified_value<2>,
        unified_value<1>
        > value;
};
template <>
inline typename accessor<0>::access_type& ycsb_value::get<0>() {
    return even_columns();
}
template <>
inline const typename accessor<0>::access_type& ycsb_value::get<0>() const {
    return even_columns();
}
template <>
inline typename accessor<1>::access_type& ycsb_value::get<1>() {
    return odd_columns();
}
template <>
inline const typename accessor<1>::access_type& ycsb_value::get<1>() const {
    return odd_columns();
}

};  // namespace ycsb_value_datatypes

using ycsb_value = ycsb_value_datatypes::ycsb_value;
using ADAPTER_OF(ycsb_value) = ADAPTER_OF(ycsb_value_datatypes::ycsb_value);

