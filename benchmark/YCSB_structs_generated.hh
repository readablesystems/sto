namespace ycsb_value_datatypes {

enum class NamedColumn : int {
    even_columns = 0,
    odd_columns,
    COLCOUNT
};

template <size_t StartIndex, size_t EndIndex>
struct split_value;

template <size_t SplitIndex>
struct unified_value;

struct ycsb_value;

CREATE_ADAPTER(ycsb_value, 2);

template <>
struct split_value<0, 1> {
    explicit split_value() = default;

    std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS> even_columns;
};
template <>
struct split_value<1, 2> {
    explicit split_value() = default;

    std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS> odd_columns;
};
template <>
struct unified_value<1> {
    std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& even_columns() {
        return split_0.even_columns;
    }
    const std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& even_columns() const {
        return split_0.even_columns;
    }
    std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& odd_columns() {
        return split_1.odd_columns;
    }
    const std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& odd_columns() const {
        return split_1.odd_columns;
    }

    split_value<0, 1> split_0;
    split_value<1, 2> split_1;
};

template <>
struct split_value<0, 2> {
    explicit split_value() = default;

    std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS> even_columns;
    std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS> odd_columns;
};
template <>
struct unified_value<2> {
    std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& even_columns() {
        return split_0.even_columns;
    }
    const std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& even_columns() const {
        return split_0.even_columns;
    }
    std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& odd_columns() {
        return split_0.odd_columns;
    }
    const std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& odd_columns() const {
        return split_0.odd_columns;
    }

    split_value<0, 2> split_0;
};

struct ycsb_value {
    explicit ycsb_value() = default;

    using NamedColumn = ycsb_value_datatypes::NamedColumn;


    std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& even_columns() {
        if (auto val = std::get_if<unified_value<1>>(&value)) {
            return val->even_columns();
        }
        return std::get<unified_value<2>>(value).even_columns();
    }
    const std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& even_columns() const {
        if (auto val = std::get_if<unified_value<1>>(&value)) {
            return val->even_columns();
        }
        return std::get<unified_value<2>>(value).even_columns();
    }
    std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& odd_columns() {
        if (auto val = std::get_if<unified_value<1>>(&value)) {
            return val->odd_columns();
        }
        return std::get<unified_value<2>>(value).odd_columns();
    }
    const std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& odd_columns() const {
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

};  // namespace ycsb_value_datatypes

using ycsb_value = ycsb_value_datatypes::ycsb_value;
using ADAPTER_OF(ycsb_value) = ADAPTER_OF(ycsb_value_datatypes::ycsb_value);

