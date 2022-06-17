#pragma once

#include <type_traits>
//#include "AdapterStructs.hh"

namespace item_row_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct item_row {
    using RecordAccessor = item_row_datatypes::RecordAccessor;
    using NamedColumn = item_row_datatypes::NamedColumn;

    var_string<100> name;
    var_string<255> description;
    uint32_t initial_price;
    uint32_t reserve_price;
    uint32_t buy_now;
    uint32_t start_date;
    uint64_t seller;
    uint64_t category;
    uint32_t quantity;
    uint32_t nb_of_bids;
    uint32_t max_bid;
    uint32_t end_date;
};

enum class NamedColumn : int {
    name = 0,
    description,
    initial_price,
    reserve_price,
    buy_now,
    start_date,
    seller,
    category,
    quantity,
    nb_of_bids,
    max_bid,
    end_date,
    COLCOUNT
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
    if constexpr (Column < NamedColumn::description) {
        return NamedColumn::name;
    }
    if constexpr (Column < NamedColumn::initial_price) {
        return NamedColumn::description;
    }
    if constexpr (Column < NamedColumn::reserve_price) {
        return NamedColumn::initial_price;
    }
    if constexpr (Column < NamedColumn::buy_now) {
        return NamedColumn::reserve_price;
    }
    if constexpr (Column < NamedColumn::start_date) {
        return NamedColumn::buy_now;
    }
    if constexpr (Column < NamedColumn::seller) {
        return NamedColumn::start_date;
    }
    if constexpr (Column < NamedColumn::category) {
        return NamedColumn::seller;
    }
    if constexpr (Column < NamedColumn::quantity) {
        return NamedColumn::category;
    }
    if constexpr (Column < NamedColumn::nb_of_bids) {
        return NamedColumn::quantity;
    }
    if constexpr (Column < NamedColumn::max_bid) {
        return NamedColumn::nb_of_bids;
    }
    if constexpr (Column < NamedColumn::end_date) {
        return NamedColumn::max_bid;
    }
    return NamedColumn::end_date;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::name> {
    using NamedColumn = item_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<100>;
    using value_type = var_string<100>;
    static constexpr NamedColumn Column = NamedColumn::name;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::description> {
    using NamedColumn = item_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<255>;
    using value_type = var_string<255>;
    static constexpr NamedColumn Column = NamedColumn::description;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::initial_price> {
    using NamedColumn = item_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::initial_price;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::reserve_price> {
    using NamedColumn = item_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::reserve_price;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::buy_now> {
    using NamedColumn = item_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::buy_now;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::start_date> {
    using NamedColumn = item_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::start_date;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::seller> {
    using NamedColumn = item_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::seller;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::category> {
    using NamedColumn = item_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint64_t;
    using value_type = uint64_t;
    static constexpr NamedColumn Column = NamedColumn::category;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::quantity> {
    using NamedColumn = item_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::quantity;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::nb_of_bids> {
    using NamedColumn = item_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::nb_of_bids;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::max_bid> {
    using NamedColumn = item_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::max_bid;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::end_date> {
    using NamedColumn = item_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::end_date;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            item_row* ptr) {
        if constexpr (NamedColumn::name <= Column && Column < NamedColumn::description) {
            return ptr->name;
        }
        if constexpr (NamedColumn::description <= Column && Column < NamedColumn::initial_price) {
            return ptr->description;
        }
        if constexpr (NamedColumn::initial_price <= Column && Column < NamedColumn::reserve_price) {
            return ptr->initial_price;
        }
        if constexpr (NamedColumn::reserve_price <= Column && Column < NamedColumn::buy_now) {
            return ptr->reserve_price;
        }
        if constexpr (NamedColumn::buy_now <= Column && Column < NamedColumn::start_date) {
            return ptr->buy_now;
        }
        if constexpr (NamedColumn::start_date <= Column && Column < NamedColumn::seller) {
            return ptr->start_date;
        }
        if constexpr (NamedColumn::seller <= Column && Column < NamedColumn::category) {
            return ptr->seller;
        }
        if constexpr (NamedColumn::category <= Column && Column < NamedColumn::quantity) {
            return ptr->category;
        }
        if constexpr (NamedColumn::quantity <= Column && Column < NamedColumn::nb_of_bids) {
            return ptr->quantity;
        }
        if constexpr (NamedColumn::nb_of_bids <= Column && Column < NamedColumn::max_bid) {
            return ptr->nb_of_bids;
        }
        if constexpr (NamedColumn::max_bid <= Column && Column < NamedColumn::end_date) {
            return ptr->max_bid;
        }
        if constexpr (NamedColumn::end_date <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->end_date;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 12;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(item_row* dest, item_row* src) {
        if constexpr(Cell == 0) {
            dest->name = src->name;
            dest->description = src->description;
            dest->initial_price = src->initial_price;
            dest->reserve_price = src->reserve_price;
            dest->buy_now = src->buy_now;
            dest->start_date = src->start_date;
            dest->seller = src->seller;
            dest->category = src->category;
            dest->quantity = src->quantity;
            dest->nb_of_bids = src->nb_of_bids;
            dest->max_bid = src->max_bid;
            dest->end_date = src->end_date;
        }
    }
};

template <>
struct SplitPolicy<1> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 1) {
            return 8;
        }
        if (cell == 0) {
            return 4;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(item_row* dest, item_row* src) {
        if constexpr(Cell == 1) {
            dest->name = src->name;
            dest->description = src->description;
            dest->initial_price = src->initial_price;
            dest->reserve_price = src->reserve_price;
            dest->buy_now = src->buy_now;
            dest->start_date = src->start_date;
            dest->seller = src->seller;
            dest->category = src->category;
        }
        if constexpr(Cell == 0) {
            dest->quantity = src->quantity;
            dest->nb_of_bids = src->nb_of_bids;
            dest->max_bid = src->max_bid;
            dest->end_date = src->end_date;
        }
    }
};

class RecordAccessor {
public:
    using NamedColumn = item_row_datatypes::NamedColumn;
    //using SplitTable = item_row_datatypes::SplitTable;
    using SplitType = item_row_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<item_row>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = item_row;
    static constexpr auto DEFAULT_SPLIT = 0;
    static constexpr auto MAX_SPLITS = 2;
    static constexpr auto MAX_POINTERS = MAX_SPLITS;
    static constexpr auto POLICIES = 2;

    RecordAccessor() = default;
    template <typename... T>
    RecordAccessor(T ...vals) : vptrs_({ pointer_of(vals)... }) {}

    inline operator bool() const {
        return vptrs_[0] != nullptr;
    }

    /*
    inline ValueType* pointer_of(ValueType& value) {
        return &value;
    }
    */

    inline ValueType* pointer_of(ValueType* vptr) {
        return vptr;
    }

    template <int Index>
    inline static constexpr const auto split_of(NamedColumn column) {
        if constexpr (Index == 0) {
            return SplitPolicy<0>::column_to_cell(column);
        }
        if constexpr (Index == 1) {
            return SplitPolicy<1>::column_to_cell(column);
        }
        return 0;
    }

    inline static constexpr const auto split_of(int index, NamedColumn column) {
        if (index == 0) {
            return SplitPolicy<0>::column_to_cell(column);
        }
        if (index == 1) {
            return SplitPolicy<1>::column_to_cell(column);
        }
        return 0;
    }

    const auto cell_of(NamedColumn column) const {
        return split_of(splitindex_, column);
    }

    template <int Index>
    inline static constexpr void copy_cell(int cell, ValueType* dest, ValueType* src) {
        if constexpr (Index >= 0 && Index < POLICIES) {
            if (cell == 0) {
                SplitPolicy<Index>::template copy_cell<0>(dest, src);
                return;
            }
            if (cell == 1) {
                SplitPolicy<Index>::template copy_cell<1>(dest, src);
                return;
            }
        } else {
            (void) dest;
            (void) src;
        }
    }

    inline static constexpr void copy_cell(int index, int cell, ValueType* dest, ValueType* src) {
        if (index == 0) {
            copy_cell<0>(cell, dest, src);
            return;
        }
        if (index == 1) {
            copy_cell<1>(cell, dest, src);
            return;
        }
    }

    inline static constexpr size_t cell_col_count(int index, int cell) {
        if (index == 0) {
            return SplitPolicy<0>::cell_col_count(cell);
        }
        if (index == 1) {
            return SplitPolicy<1>::cell_col_count(cell);
        }
        return 0;
    }

    void copy_into(item_row* vptr, int index=0) {
        copy_cell(index, 0, vptr, vptrs_[0]);
        copy_cell(index, 1, vptr, vptrs_[1]);
    }

    inline typename accessor_info<NamedColumn::name>::value_type& name() {
        return vptrs_[cell_of(NamedColumn::name)]->name;
    }

    inline const typename accessor_info<NamedColumn::name>::value_type& name() const {
        return vptrs_[cell_of(NamedColumn::name)]->name;
    }

    inline typename accessor_info<NamedColumn::description>::value_type& description() {
        return vptrs_[cell_of(NamedColumn::description)]->description;
    }

    inline const typename accessor_info<NamedColumn::description>::value_type& description() const {
        return vptrs_[cell_of(NamedColumn::description)]->description;
    }

    inline typename accessor_info<NamedColumn::initial_price>::value_type& initial_price() {
        return vptrs_[cell_of(NamedColumn::initial_price)]->initial_price;
    }

    inline const typename accessor_info<NamedColumn::initial_price>::value_type& initial_price() const {
        return vptrs_[cell_of(NamedColumn::initial_price)]->initial_price;
    }

    inline typename accessor_info<NamedColumn::reserve_price>::value_type& reserve_price() {
        return vptrs_[cell_of(NamedColumn::reserve_price)]->reserve_price;
    }

    inline const typename accessor_info<NamedColumn::reserve_price>::value_type& reserve_price() const {
        return vptrs_[cell_of(NamedColumn::reserve_price)]->reserve_price;
    }

    inline typename accessor_info<NamedColumn::buy_now>::value_type& buy_now() {
        return vptrs_[cell_of(NamedColumn::buy_now)]->buy_now;
    }

    inline const typename accessor_info<NamedColumn::buy_now>::value_type& buy_now() const {
        return vptrs_[cell_of(NamedColumn::buy_now)]->buy_now;
    }

    inline typename accessor_info<NamedColumn::start_date>::value_type& start_date() {
        return vptrs_[cell_of(NamedColumn::start_date)]->start_date;
    }

    inline const typename accessor_info<NamedColumn::start_date>::value_type& start_date() const {
        return vptrs_[cell_of(NamedColumn::start_date)]->start_date;
    }

    inline typename accessor_info<NamedColumn::seller>::value_type& seller() {
        return vptrs_[cell_of(NamedColumn::seller)]->seller;
    }

    inline const typename accessor_info<NamedColumn::seller>::value_type& seller() const {
        return vptrs_[cell_of(NamedColumn::seller)]->seller;
    }

    inline typename accessor_info<NamedColumn::category>::value_type& category() {
        return vptrs_[cell_of(NamedColumn::category)]->category;
    }

    inline const typename accessor_info<NamedColumn::category>::value_type& category() const {
        return vptrs_[cell_of(NamedColumn::category)]->category;
    }

    inline typename accessor_info<NamedColumn::quantity>::value_type& quantity() {
        return vptrs_[cell_of(NamedColumn::quantity)]->quantity;
    }

    inline const typename accessor_info<NamedColumn::quantity>::value_type& quantity() const {
        return vptrs_[cell_of(NamedColumn::quantity)]->quantity;
    }

    inline typename accessor_info<NamedColumn::nb_of_bids>::value_type& nb_of_bids() {
        return vptrs_[cell_of(NamedColumn::nb_of_bids)]->nb_of_bids;
    }

    inline const typename accessor_info<NamedColumn::nb_of_bids>::value_type& nb_of_bids() const {
        return vptrs_[cell_of(NamedColumn::nb_of_bids)]->nb_of_bids;
    }

    inline typename accessor_info<NamedColumn::max_bid>::value_type& max_bid() {
        return vptrs_[cell_of(NamedColumn::max_bid)]->max_bid;
    }

    inline const typename accessor_info<NamedColumn::max_bid>::value_type& max_bid() const {
        return vptrs_[cell_of(NamedColumn::max_bid)]->max_bid;
    }

    inline typename accessor_info<NamedColumn::end_date>::value_type& end_date() {
        return vptrs_[cell_of(NamedColumn::end_date)]->end_date;
    }

    inline const typename accessor_info<NamedColumn::end_date>::value_type& end_date() const {
        return vptrs_[cell_of(NamedColumn::end_date)]->end_date;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::name <= Column && Column < NamedColumn::description) {
            return vptrs_[cell_of(NamedColumn::name)]->name;
        }
        if constexpr (NamedColumn::description <= Column && Column < NamedColumn::initial_price) {
            return vptrs_[cell_of(NamedColumn::description)]->description;
        }
        if constexpr (NamedColumn::initial_price <= Column && Column < NamedColumn::reserve_price) {
            return vptrs_[cell_of(NamedColumn::initial_price)]->initial_price;
        }
        if constexpr (NamedColumn::reserve_price <= Column && Column < NamedColumn::buy_now) {
            return vptrs_[cell_of(NamedColumn::reserve_price)]->reserve_price;
        }
        if constexpr (NamedColumn::buy_now <= Column && Column < NamedColumn::start_date) {
            return vptrs_[cell_of(NamedColumn::buy_now)]->buy_now;
        }
        if constexpr (NamedColumn::start_date <= Column && Column < NamedColumn::seller) {
            return vptrs_[cell_of(NamedColumn::start_date)]->start_date;
        }
        if constexpr (NamedColumn::seller <= Column && Column < NamedColumn::category) {
            return vptrs_[cell_of(NamedColumn::seller)]->seller;
        }
        if constexpr (NamedColumn::category <= Column && Column < NamedColumn::quantity) {
            return vptrs_[cell_of(NamedColumn::category)]->category;
        }
        if constexpr (NamedColumn::quantity <= Column && Column < NamedColumn::nb_of_bids) {
            return vptrs_[cell_of(NamedColumn::quantity)]->quantity;
        }
        if constexpr (NamedColumn::nb_of_bids <= Column && Column < NamedColumn::max_bid) {
            return vptrs_[cell_of(NamedColumn::nb_of_bids)]->nb_of_bids;
        }
        if constexpr (NamedColumn::max_bid <= Column && Column < NamedColumn::end_date) {
            return vptrs_[cell_of(NamedColumn::max_bid)]->max_bid;
        }
        if constexpr (NamedColumn::end_date <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::end_date)]->end_date;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<item_row*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace item_row_datatypes

using item_row = item_row_datatypes::item_row;

namespace bid_row_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct bid_row {
    using RecordAccessor = bid_row_datatypes::RecordAccessor;
    using NamedColumn = bid_row_datatypes::NamedColumn;

    uint32_t quantity;
    uint32_t bid;
    uint32_t max_bid;
    uint32_t date;
};

enum class NamedColumn : int {
    quantity = 0,
    bid,
    max_bid,
    date,
    COLCOUNT
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
    if constexpr (Column < NamedColumn::bid) {
        return NamedColumn::quantity;
    }
    if constexpr (Column < NamedColumn::max_bid) {
        return NamedColumn::bid;
    }
    if constexpr (Column < NamedColumn::date) {
        return NamedColumn::max_bid;
    }
    return NamedColumn::date;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::quantity> {
    using NamedColumn = bid_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::quantity;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::bid> {
    using NamedColumn = bid_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::bid;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::max_bid> {
    using NamedColumn = bid_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::max_bid;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::date> {
    using NamedColumn = bid_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::date;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            bid_row* ptr) {
        if constexpr (NamedColumn::quantity <= Column && Column < NamedColumn::bid) {
            return ptr->quantity;
        }
        if constexpr (NamedColumn::bid <= Column && Column < NamedColumn::max_bid) {
            return ptr->bid;
        }
        if constexpr (NamedColumn::max_bid <= Column && Column < NamedColumn::date) {
            return ptr->max_bid;
        }
        if constexpr (NamedColumn::date <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->date;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 4;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(bid_row* dest, bid_row* src) {
        if constexpr(Cell == 0) {
            dest->quantity = src->quantity;
            dest->bid = src->bid;
            dest->max_bid = src->max_bid;
            dest->date = src->date;
        }
    }
};

class RecordAccessor {
public:
    using NamedColumn = bid_row_datatypes::NamedColumn;
    //using SplitTable = bid_row_datatypes::SplitTable;
    using SplitType = bid_row_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<bid_row>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = bid_row;
    static constexpr auto DEFAULT_SPLIT = 0;
    static constexpr auto MAX_SPLITS = 2;
    static constexpr auto MAX_POINTERS = MAX_SPLITS;
    static constexpr auto POLICIES = 1;

    RecordAccessor() = default;
    template <typename... T>
    RecordAccessor(T ...vals) : vptrs_({ pointer_of(vals)... }) {}

    inline operator bool() const {
        return vptrs_[0] != nullptr;
    }

    /*
    inline ValueType* pointer_of(ValueType& value) {
        return &value;
    }
    */

    inline ValueType* pointer_of(ValueType* vptr) {
        return vptr;
    }

    template <int Index>
    inline static constexpr const auto split_of(NamedColumn column) {
        if constexpr (Index == 0) {
            return SplitPolicy<0>::column_to_cell(column);
        }
        return 0;
    }

    inline static constexpr const auto split_of(int index, NamedColumn column) {
        if (index == 0) {
            return SplitPolicy<0>::column_to_cell(column);
        }
        return 0;
    }

    const auto cell_of(NamedColumn column) const {
        return split_of(splitindex_, column);
    }

    template <int Index>
    inline static constexpr void copy_cell(int cell, ValueType* dest, ValueType* src) {
        if constexpr (Index >= 0 && Index < POLICIES) {
            if (cell == 0) {
                SplitPolicy<Index>::template copy_cell<0>(dest, src);
                return;
            }
        } else {
            (void) dest;
            (void) src;
        }
    }

    inline static constexpr void copy_cell(int index, int cell, ValueType* dest, ValueType* src) {
        if (index == 0) {
            copy_cell<0>(cell, dest, src);
            return;
        }
    }

    inline static constexpr size_t cell_col_count(int index, int cell) {
        if (index == 0) {
            return SplitPolicy<0>::cell_col_count(cell);
        }
        return 0;
    }

    void copy_into(bid_row* vptr, int index=0) {
        copy_cell(index, 0, vptr, vptrs_[0]);
    }

    inline typename accessor_info<NamedColumn::quantity>::value_type& quantity() {
        return vptrs_[cell_of(NamedColumn::quantity)]->quantity;
    }

    inline const typename accessor_info<NamedColumn::quantity>::value_type& quantity() const {
        return vptrs_[cell_of(NamedColumn::quantity)]->quantity;
    }

    inline typename accessor_info<NamedColumn::bid>::value_type& bid() {
        return vptrs_[cell_of(NamedColumn::bid)]->bid;
    }

    inline const typename accessor_info<NamedColumn::bid>::value_type& bid() const {
        return vptrs_[cell_of(NamedColumn::bid)]->bid;
    }

    inline typename accessor_info<NamedColumn::max_bid>::value_type& max_bid() {
        return vptrs_[cell_of(NamedColumn::max_bid)]->max_bid;
    }

    inline const typename accessor_info<NamedColumn::max_bid>::value_type& max_bid() const {
        return vptrs_[cell_of(NamedColumn::max_bid)]->max_bid;
    }

    inline typename accessor_info<NamedColumn::date>::value_type& date() {
        return vptrs_[cell_of(NamedColumn::date)]->date;
    }

    inline const typename accessor_info<NamedColumn::date>::value_type& date() const {
        return vptrs_[cell_of(NamedColumn::date)]->date;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::quantity <= Column && Column < NamedColumn::bid) {
            return vptrs_[cell_of(NamedColumn::quantity)]->quantity;
        }
        if constexpr (NamedColumn::bid <= Column && Column < NamedColumn::max_bid) {
            return vptrs_[cell_of(NamedColumn::bid)]->bid;
        }
        if constexpr (NamedColumn::max_bid <= Column && Column < NamedColumn::date) {
            return vptrs_[cell_of(NamedColumn::max_bid)]->max_bid;
        }
        if constexpr (NamedColumn::date <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::date)]->date;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<bid_row*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace bid_row_datatypes

using bid_row = bid_row_datatypes::bid_row;

namespace buynow_row_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct buynow_row {
    using RecordAccessor = buynow_row_datatypes::RecordAccessor;
    using NamedColumn = buynow_row_datatypes::NamedColumn;

    uint32_t quantity;
    uint32_t date;
};

enum class NamedColumn : int {
    quantity = 0,
    date,
    COLCOUNT
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
    if constexpr (Column < NamedColumn::date) {
        return NamedColumn::quantity;
    }
    return NamedColumn::date;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::quantity> {
    using NamedColumn = buynow_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::quantity;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::date> {
    using NamedColumn = buynow_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint32_t;
    using value_type = uint32_t;
    static constexpr NamedColumn Column = NamedColumn::date;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            buynow_row* ptr) {
        if constexpr (NamedColumn::quantity <= Column && Column < NamedColumn::date) {
            return ptr->quantity;
        }
        if constexpr (NamedColumn::date <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->date;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 2;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(buynow_row* dest, buynow_row* src) {
        if constexpr(Cell == 0) {
            dest->quantity = src->quantity;
            dest->date = src->date;
        }
    }
};

class RecordAccessor {
public:
    using NamedColumn = buynow_row_datatypes::NamedColumn;
    //using SplitTable = buynow_row_datatypes::SplitTable;
    using SplitType = buynow_row_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<buynow_row>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = buynow_row;
    static constexpr auto DEFAULT_SPLIT = 0;
    static constexpr auto MAX_SPLITS = 2;
    static constexpr auto MAX_POINTERS = MAX_SPLITS;
    static constexpr auto POLICIES = 1;

    RecordAccessor() = default;
    template <typename... T>
    RecordAccessor(T ...vals) : vptrs_({ pointer_of(vals)... }) {}

    inline operator bool() const {
        return vptrs_[0] != nullptr;
    }

    /*
    inline ValueType* pointer_of(ValueType& value) {
        return &value;
    }
    */

    inline ValueType* pointer_of(ValueType* vptr) {
        return vptr;
    }

    template <int Index>
    inline static constexpr const auto split_of(NamedColumn column) {
        if constexpr (Index == 0) {
            return SplitPolicy<0>::column_to_cell(column);
        }
        return 0;
    }

    inline static constexpr const auto split_of(int index, NamedColumn column) {
        if (index == 0) {
            return SplitPolicy<0>::column_to_cell(column);
        }
        return 0;
    }

    const auto cell_of(NamedColumn column) const {
        return split_of(splitindex_, column);
    }

    template <int Index>
    inline static constexpr void copy_cell(int cell, ValueType* dest, ValueType* src) {
        if constexpr (Index >= 0 && Index < POLICIES) {
            if (cell == 0) {
                SplitPolicy<Index>::template copy_cell<0>(dest, src);
                return;
            }
        } else {
            (void) dest;
            (void) src;
        }
    }

    inline static constexpr void copy_cell(int index, int cell, ValueType* dest, ValueType* src) {
        if (index == 0) {
            copy_cell<0>(cell, dest, src);
            return;
        }
    }

    inline static constexpr size_t cell_col_count(int index, int cell) {
        if (index == 0) {
            return SplitPolicy<0>::cell_col_count(cell);
        }
        return 0;
    }

    void copy_into(buynow_row* vptr, int index=0) {
        copy_cell(index, 0, vptr, vptrs_[0]);
    }

    inline typename accessor_info<NamedColumn::quantity>::value_type& quantity() {
        return vptrs_[cell_of(NamedColumn::quantity)]->quantity;
    }

    inline const typename accessor_info<NamedColumn::quantity>::value_type& quantity() const {
        return vptrs_[cell_of(NamedColumn::quantity)]->quantity;
    }

    inline typename accessor_info<NamedColumn::date>::value_type& date() {
        return vptrs_[cell_of(NamedColumn::date)]->date;
    }

    inline const typename accessor_info<NamedColumn::date>::value_type& date() const {
        return vptrs_[cell_of(NamedColumn::date)]->date;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::quantity <= Column && Column < NamedColumn::date) {
            return vptrs_[cell_of(NamedColumn::quantity)]->quantity;
        }
        if constexpr (NamedColumn::date <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::date)]->date;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<buynow_row*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace buynow_row_datatypes

using buynow_row = buynow_row_datatypes::buynow_row;

