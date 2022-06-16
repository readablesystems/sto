#pragma once

#include <type_traits>
//#include "AdapterStructs.hh"

namespace ipblocks_row_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct ipblocks_row {
    using RecordAccessor = ipblocks_row_datatypes::RecordAccessor;
    using NamedColumn = ipblocks_row_datatypes::NamedColumn;

    var_string<255> ipb_address;
    int32_t ipb_user;
    int32_t ipb_by;
    var_string<255> ipb_by_text;
    var_string<255> ipb_reason;
    var_string<14> ipb_timestamp;
    int32_t ipb_auto;
    int32_t ipb_anon_only;
    int32_t ipb_create_account;
    int32_t ipb_enable_autoblock;
    var_string<14> ipb_expiry;
    var_string<8> ipb_range_start;
    var_string<8> ipb_range_end;
    int32_t ipb_deleted;
    int32_t ipb_block_email;
    int32_t ipb_allow_usertalk;
};

enum class NamedColumn : int {
    ipb_address = 0,
    ipb_user,
    ipb_by,
    ipb_by_text,
    ipb_reason,
    ipb_timestamp,
    ipb_auto,
    ipb_anon_only,
    ipb_create_account,
    ipb_enable_autoblock,
    ipb_expiry,
    ipb_range_start,
    ipb_range_end,
    ipb_deleted,
    ipb_block_email,
    ipb_allow_usertalk,
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
    if constexpr (Column < NamedColumn::ipb_user) {
        return NamedColumn::ipb_address;
    }
    if constexpr (Column < NamedColumn::ipb_by) {
        return NamedColumn::ipb_user;
    }
    if constexpr (Column < NamedColumn::ipb_by_text) {
        return NamedColumn::ipb_by;
    }
    if constexpr (Column < NamedColumn::ipb_reason) {
        return NamedColumn::ipb_by_text;
    }
    if constexpr (Column < NamedColumn::ipb_timestamp) {
        return NamedColumn::ipb_reason;
    }
    if constexpr (Column < NamedColumn::ipb_auto) {
        return NamedColumn::ipb_timestamp;
    }
    if constexpr (Column < NamedColumn::ipb_anon_only) {
        return NamedColumn::ipb_auto;
    }
    if constexpr (Column < NamedColumn::ipb_create_account) {
        return NamedColumn::ipb_anon_only;
    }
    if constexpr (Column < NamedColumn::ipb_enable_autoblock) {
        return NamedColumn::ipb_create_account;
    }
    if constexpr (Column < NamedColumn::ipb_expiry) {
        return NamedColumn::ipb_enable_autoblock;
    }
    if constexpr (Column < NamedColumn::ipb_range_start) {
        return NamedColumn::ipb_expiry;
    }
    if constexpr (Column < NamedColumn::ipb_range_end) {
        return NamedColumn::ipb_range_start;
    }
    if constexpr (Column < NamedColumn::ipb_deleted) {
        return NamedColumn::ipb_range_end;
    }
    if constexpr (Column < NamedColumn::ipb_block_email) {
        return NamedColumn::ipb_deleted;
    }
    if constexpr (Column < NamedColumn::ipb_allow_usertalk) {
        return NamedColumn::ipb_block_email;
    }
    return NamedColumn::ipb_allow_usertalk;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::ipb_address> {
    using NamedColumn = ipblocks_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<255>;
    using value_type = var_string<255>;
    static constexpr NamedColumn Column = NamedColumn::ipb_address;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::ipb_user> {
    using NamedColumn = ipblocks_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::ipb_user;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::ipb_by> {
    using NamedColumn = ipblocks_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::ipb_by;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::ipb_by_text> {
    using NamedColumn = ipblocks_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<255>;
    using value_type = var_string<255>;
    static constexpr NamedColumn Column = NamedColumn::ipb_by_text;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::ipb_reason> {
    using NamedColumn = ipblocks_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<255>;
    using value_type = var_string<255>;
    static constexpr NamedColumn Column = NamedColumn::ipb_reason;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::ipb_timestamp> {
    using NamedColumn = ipblocks_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<14>;
    using value_type = var_string<14>;
    static constexpr NamedColumn Column = NamedColumn::ipb_timestamp;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::ipb_auto> {
    using NamedColumn = ipblocks_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::ipb_auto;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::ipb_anon_only> {
    using NamedColumn = ipblocks_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::ipb_anon_only;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::ipb_create_account> {
    using NamedColumn = ipblocks_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::ipb_create_account;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::ipb_enable_autoblock> {
    using NamedColumn = ipblocks_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::ipb_enable_autoblock;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::ipb_expiry> {
    using NamedColumn = ipblocks_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<14>;
    using value_type = var_string<14>;
    static constexpr NamedColumn Column = NamedColumn::ipb_expiry;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::ipb_range_start> {
    using NamedColumn = ipblocks_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<8>;
    using value_type = var_string<8>;
    static constexpr NamedColumn Column = NamedColumn::ipb_range_start;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::ipb_range_end> {
    using NamedColumn = ipblocks_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<8>;
    using value_type = var_string<8>;
    static constexpr NamedColumn Column = NamedColumn::ipb_range_end;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::ipb_deleted> {
    using NamedColumn = ipblocks_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::ipb_deleted;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::ipb_block_email> {
    using NamedColumn = ipblocks_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::ipb_block_email;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::ipb_allow_usertalk> {
    using NamedColumn = ipblocks_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::ipb_allow_usertalk;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            ipblocks_row* ptr) {
        if constexpr (NamedColumn::ipb_address <= Column && Column < NamedColumn::ipb_user) {
            return ptr->ipb_address;
        }
        if constexpr (NamedColumn::ipb_user <= Column && Column < NamedColumn::ipb_by) {
            return ptr->ipb_user;
        }
        if constexpr (NamedColumn::ipb_by <= Column && Column < NamedColumn::ipb_by_text) {
            return ptr->ipb_by;
        }
        if constexpr (NamedColumn::ipb_by_text <= Column && Column < NamedColumn::ipb_reason) {
            return ptr->ipb_by_text;
        }
        if constexpr (NamedColumn::ipb_reason <= Column && Column < NamedColumn::ipb_timestamp) {
            return ptr->ipb_reason;
        }
        if constexpr (NamedColumn::ipb_timestamp <= Column && Column < NamedColumn::ipb_auto) {
            return ptr->ipb_timestamp;
        }
        if constexpr (NamedColumn::ipb_auto <= Column && Column < NamedColumn::ipb_anon_only) {
            return ptr->ipb_auto;
        }
        if constexpr (NamedColumn::ipb_anon_only <= Column && Column < NamedColumn::ipb_create_account) {
            return ptr->ipb_anon_only;
        }
        if constexpr (NamedColumn::ipb_create_account <= Column && Column < NamedColumn::ipb_enable_autoblock) {
            return ptr->ipb_create_account;
        }
        if constexpr (NamedColumn::ipb_enable_autoblock <= Column && Column < NamedColumn::ipb_expiry) {
            return ptr->ipb_enable_autoblock;
        }
        if constexpr (NamedColumn::ipb_expiry <= Column && Column < NamedColumn::ipb_range_start) {
            return ptr->ipb_expiry;
        }
        if constexpr (NamedColumn::ipb_range_start <= Column && Column < NamedColumn::ipb_range_end) {
            return ptr->ipb_range_start;
        }
        if constexpr (NamedColumn::ipb_range_end <= Column && Column < NamedColumn::ipb_deleted) {
            return ptr->ipb_range_end;
        }
        if constexpr (NamedColumn::ipb_deleted <= Column && Column < NamedColumn::ipb_block_email) {
            return ptr->ipb_deleted;
        }
        if constexpr (NamedColumn::ipb_block_email <= Column && Column < NamedColumn::ipb_allow_usertalk) {
            return ptr->ipb_block_email;
        }
        if constexpr (NamedColumn::ipb_allow_usertalk <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->ipb_allow_usertalk;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 16;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(ipblocks_row* dest, ipblocks_row* src) {
        if constexpr(Cell == 0) {
            dest->ipb_address = src->ipb_address;
            dest->ipb_user = src->ipb_user;
            dest->ipb_by = src->ipb_by;
            dest->ipb_by_text = src->ipb_by_text;
            dest->ipb_reason = src->ipb_reason;
            dest->ipb_timestamp = src->ipb_timestamp;
            dest->ipb_auto = src->ipb_auto;
            dest->ipb_anon_only = src->ipb_anon_only;
            dest->ipb_create_account = src->ipb_create_account;
            dest->ipb_enable_autoblock = src->ipb_enable_autoblock;
            dest->ipb_expiry = src->ipb_expiry;
            dest->ipb_range_start = src->ipb_range_start;
            dest->ipb_range_end = src->ipb_range_end;
            dest->ipb_deleted = src->ipb_deleted;
            dest->ipb_block_email = src->ipb_block_email;
            dest->ipb_allow_usertalk = src->ipb_allow_usertalk;
        }
    }
};

class RecordAccessor {
public:
    using NamedColumn = ipblocks_row_datatypes::NamedColumn;
    //using SplitTable = ipblocks_row_datatypes::SplitTable;
    using SplitType = ipblocks_row_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<ipblocks_row>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = ipblocks_row;
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

    void copy_into(ipblocks_row* vptr, int index=0) {
        copy_cell(index, 0, vptr, vptrs_[0]);
    }

    inline typename accessor_info<NamedColumn::ipb_address>::value_type& ipb_address() {
        return vptrs_[cell_of(NamedColumn::ipb_address)]->ipb_address;
    }

    inline const typename accessor_info<NamedColumn::ipb_address>::value_type& ipb_address() const {
        return vptrs_[cell_of(NamedColumn::ipb_address)]->ipb_address;
    }

    inline typename accessor_info<NamedColumn::ipb_user>::value_type& ipb_user() {
        return vptrs_[cell_of(NamedColumn::ipb_user)]->ipb_user;
    }

    inline const typename accessor_info<NamedColumn::ipb_user>::value_type& ipb_user() const {
        return vptrs_[cell_of(NamedColumn::ipb_user)]->ipb_user;
    }

    inline typename accessor_info<NamedColumn::ipb_by>::value_type& ipb_by() {
        return vptrs_[cell_of(NamedColumn::ipb_by)]->ipb_by;
    }

    inline const typename accessor_info<NamedColumn::ipb_by>::value_type& ipb_by() const {
        return vptrs_[cell_of(NamedColumn::ipb_by)]->ipb_by;
    }

    inline typename accessor_info<NamedColumn::ipb_by_text>::value_type& ipb_by_text() {
        return vptrs_[cell_of(NamedColumn::ipb_by_text)]->ipb_by_text;
    }

    inline const typename accessor_info<NamedColumn::ipb_by_text>::value_type& ipb_by_text() const {
        return vptrs_[cell_of(NamedColumn::ipb_by_text)]->ipb_by_text;
    }

    inline typename accessor_info<NamedColumn::ipb_reason>::value_type& ipb_reason() {
        return vptrs_[cell_of(NamedColumn::ipb_reason)]->ipb_reason;
    }

    inline const typename accessor_info<NamedColumn::ipb_reason>::value_type& ipb_reason() const {
        return vptrs_[cell_of(NamedColumn::ipb_reason)]->ipb_reason;
    }

    inline typename accessor_info<NamedColumn::ipb_timestamp>::value_type& ipb_timestamp() {
        return vptrs_[cell_of(NamedColumn::ipb_timestamp)]->ipb_timestamp;
    }

    inline const typename accessor_info<NamedColumn::ipb_timestamp>::value_type& ipb_timestamp() const {
        return vptrs_[cell_of(NamedColumn::ipb_timestamp)]->ipb_timestamp;
    }

    inline typename accessor_info<NamedColumn::ipb_auto>::value_type& ipb_auto() {
        return vptrs_[cell_of(NamedColumn::ipb_auto)]->ipb_auto;
    }

    inline const typename accessor_info<NamedColumn::ipb_auto>::value_type& ipb_auto() const {
        return vptrs_[cell_of(NamedColumn::ipb_auto)]->ipb_auto;
    }

    inline typename accessor_info<NamedColumn::ipb_anon_only>::value_type& ipb_anon_only() {
        return vptrs_[cell_of(NamedColumn::ipb_anon_only)]->ipb_anon_only;
    }

    inline const typename accessor_info<NamedColumn::ipb_anon_only>::value_type& ipb_anon_only() const {
        return vptrs_[cell_of(NamedColumn::ipb_anon_only)]->ipb_anon_only;
    }

    inline typename accessor_info<NamedColumn::ipb_create_account>::value_type& ipb_create_account() {
        return vptrs_[cell_of(NamedColumn::ipb_create_account)]->ipb_create_account;
    }

    inline const typename accessor_info<NamedColumn::ipb_create_account>::value_type& ipb_create_account() const {
        return vptrs_[cell_of(NamedColumn::ipb_create_account)]->ipb_create_account;
    }

    inline typename accessor_info<NamedColumn::ipb_enable_autoblock>::value_type& ipb_enable_autoblock() {
        return vptrs_[cell_of(NamedColumn::ipb_enable_autoblock)]->ipb_enable_autoblock;
    }

    inline const typename accessor_info<NamedColumn::ipb_enable_autoblock>::value_type& ipb_enable_autoblock() const {
        return vptrs_[cell_of(NamedColumn::ipb_enable_autoblock)]->ipb_enable_autoblock;
    }

    inline typename accessor_info<NamedColumn::ipb_expiry>::value_type& ipb_expiry() {
        return vptrs_[cell_of(NamedColumn::ipb_expiry)]->ipb_expiry;
    }

    inline const typename accessor_info<NamedColumn::ipb_expiry>::value_type& ipb_expiry() const {
        return vptrs_[cell_of(NamedColumn::ipb_expiry)]->ipb_expiry;
    }

    inline typename accessor_info<NamedColumn::ipb_range_start>::value_type& ipb_range_start() {
        return vptrs_[cell_of(NamedColumn::ipb_range_start)]->ipb_range_start;
    }

    inline const typename accessor_info<NamedColumn::ipb_range_start>::value_type& ipb_range_start() const {
        return vptrs_[cell_of(NamedColumn::ipb_range_start)]->ipb_range_start;
    }

    inline typename accessor_info<NamedColumn::ipb_range_end>::value_type& ipb_range_end() {
        return vptrs_[cell_of(NamedColumn::ipb_range_end)]->ipb_range_end;
    }

    inline const typename accessor_info<NamedColumn::ipb_range_end>::value_type& ipb_range_end() const {
        return vptrs_[cell_of(NamedColumn::ipb_range_end)]->ipb_range_end;
    }

    inline typename accessor_info<NamedColumn::ipb_deleted>::value_type& ipb_deleted() {
        return vptrs_[cell_of(NamedColumn::ipb_deleted)]->ipb_deleted;
    }

    inline const typename accessor_info<NamedColumn::ipb_deleted>::value_type& ipb_deleted() const {
        return vptrs_[cell_of(NamedColumn::ipb_deleted)]->ipb_deleted;
    }

    inline typename accessor_info<NamedColumn::ipb_block_email>::value_type& ipb_block_email() {
        return vptrs_[cell_of(NamedColumn::ipb_block_email)]->ipb_block_email;
    }

    inline const typename accessor_info<NamedColumn::ipb_block_email>::value_type& ipb_block_email() const {
        return vptrs_[cell_of(NamedColumn::ipb_block_email)]->ipb_block_email;
    }

    inline typename accessor_info<NamedColumn::ipb_allow_usertalk>::value_type& ipb_allow_usertalk() {
        return vptrs_[cell_of(NamedColumn::ipb_allow_usertalk)]->ipb_allow_usertalk;
    }

    inline const typename accessor_info<NamedColumn::ipb_allow_usertalk>::value_type& ipb_allow_usertalk() const {
        return vptrs_[cell_of(NamedColumn::ipb_allow_usertalk)]->ipb_allow_usertalk;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::ipb_address <= Column && Column < NamedColumn::ipb_user) {
            return vptrs_[cell_of(NamedColumn::ipb_address)]->ipb_address;
        }
        if constexpr (NamedColumn::ipb_user <= Column && Column < NamedColumn::ipb_by) {
            return vptrs_[cell_of(NamedColumn::ipb_user)]->ipb_user;
        }
        if constexpr (NamedColumn::ipb_by <= Column && Column < NamedColumn::ipb_by_text) {
            return vptrs_[cell_of(NamedColumn::ipb_by)]->ipb_by;
        }
        if constexpr (NamedColumn::ipb_by_text <= Column && Column < NamedColumn::ipb_reason) {
            return vptrs_[cell_of(NamedColumn::ipb_by_text)]->ipb_by_text;
        }
        if constexpr (NamedColumn::ipb_reason <= Column && Column < NamedColumn::ipb_timestamp) {
            return vptrs_[cell_of(NamedColumn::ipb_reason)]->ipb_reason;
        }
        if constexpr (NamedColumn::ipb_timestamp <= Column && Column < NamedColumn::ipb_auto) {
            return vptrs_[cell_of(NamedColumn::ipb_timestamp)]->ipb_timestamp;
        }
        if constexpr (NamedColumn::ipb_auto <= Column && Column < NamedColumn::ipb_anon_only) {
            return vptrs_[cell_of(NamedColumn::ipb_auto)]->ipb_auto;
        }
        if constexpr (NamedColumn::ipb_anon_only <= Column && Column < NamedColumn::ipb_create_account) {
            return vptrs_[cell_of(NamedColumn::ipb_anon_only)]->ipb_anon_only;
        }
        if constexpr (NamedColumn::ipb_create_account <= Column && Column < NamedColumn::ipb_enable_autoblock) {
            return vptrs_[cell_of(NamedColumn::ipb_create_account)]->ipb_create_account;
        }
        if constexpr (NamedColumn::ipb_enable_autoblock <= Column && Column < NamedColumn::ipb_expiry) {
            return vptrs_[cell_of(NamedColumn::ipb_enable_autoblock)]->ipb_enable_autoblock;
        }
        if constexpr (NamedColumn::ipb_expiry <= Column && Column < NamedColumn::ipb_range_start) {
            return vptrs_[cell_of(NamedColumn::ipb_expiry)]->ipb_expiry;
        }
        if constexpr (NamedColumn::ipb_range_start <= Column && Column < NamedColumn::ipb_range_end) {
            return vptrs_[cell_of(NamedColumn::ipb_range_start)]->ipb_range_start;
        }
        if constexpr (NamedColumn::ipb_range_end <= Column && Column < NamedColumn::ipb_deleted) {
            return vptrs_[cell_of(NamedColumn::ipb_range_end)]->ipb_range_end;
        }
        if constexpr (NamedColumn::ipb_deleted <= Column && Column < NamedColumn::ipb_block_email) {
            return vptrs_[cell_of(NamedColumn::ipb_deleted)]->ipb_deleted;
        }
        if constexpr (NamedColumn::ipb_block_email <= Column && Column < NamedColumn::ipb_allow_usertalk) {
            return vptrs_[cell_of(NamedColumn::ipb_block_email)]->ipb_block_email;
        }
        if constexpr (NamedColumn::ipb_allow_usertalk <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::ipb_allow_usertalk)]->ipb_allow_usertalk;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<ipblocks_row*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace ipblocks_row_datatypes

using ipblocks_row = ipblocks_row_datatypes::ipblocks_row;

namespace logging_row_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct logging_row {
    using RecordAccessor = logging_row_datatypes::RecordAccessor;
    using NamedColumn = logging_row_datatypes::NamedColumn;

    var_string<32> log_type;
    var_string<32> log_action;
    var_string<14> log_timestamp;
    int32_t log_user;
    int32_t log_namespace;
    var_string<255> log_title;
    var_string<255> log_comment;
    var_string<255> log_params;
    int32_t log_deleted;
    var_string<255> log_user_text;
    int32_t log_page;
};

enum class NamedColumn : int {
    log_type = 0,
    log_action,
    log_timestamp,
    log_user,
    log_namespace,
    log_title,
    log_comment,
    log_params,
    log_deleted,
    log_user_text,
    log_page,
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
    if constexpr (Column < NamedColumn::log_action) {
        return NamedColumn::log_type;
    }
    if constexpr (Column < NamedColumn::log_timestamp) {
        return NamedColumn::log_action;
    }
    if constexpr (Column < NamedColumn::log_user) {
        return NamedColumn::log_timestamp;
    }
    if constexpr (Column < NamedColumn::log_namespace) {
        return NamedColumn::log_user;
    }
    if constexpr (Column < NamedColumn::log_title) {
        return NamedColumn::log_namespace;
    }
    if constexpr (Column < NamedColumn::log_comment) {
        return NamedColumn::log_title;
    }
    if constexpr (Column < NamedColumn::log_params) {
        return NamedColumn::log_comment;
    }
    if constexpr (Column < NamedColumn::log_deleted) {
        return NamedColumn::log_params;
    }
    if constexpr (Column < NamedColumn::log_user_text) {
        return NamedColumn::log_deleted;
    }
    if constexpr (Column < NamedColumn::log_page) {
        return NamedColumn::log_user_text;
    }
    return NamedColumn::log_page;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::log_type> {
    using NamedColumn = logging_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<32>;
    using value_type = var_string<32>;
    static constexpr NamedColumn Column = NamedColumn::log_type;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::log_action> {
    using NamedColumn = logging_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<32>;
    using value_type = var_string<32>;
    static constexpr NamedColumn Column = NamedColumn::log_action;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::log_timestamp> {
    using NamedColumn = logging_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<14>;
    using value_type = var_string<14>;
    static constexpr NamedColumn Column = NamedColumn::log_timestamp;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::log_user> {
    using NamedColumn = logging_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::log_user;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::log_namespace> {
    using NamedColumn = logging_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::log_namespace;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::log_title> {
    using NamedColumn = logging_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<255>;
    using value_type = var_string<255>;
    static constexpr NamedColumn Column = NamedColumn::log_title;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::log_comment> {
    using NamedColumn = logging_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<255>;
    using value_type = var_string<255>;
    static constexpr NamedColumn Column = NamedColumn::log_comment;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::log_params> {
    using NamedColumn = logging_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<255>;
    using value_type = var_string<255>;
    static constexpr NamedColumn Column = NamedColumn::log_params;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::log_deleted> {
    using NamedColumn = logging_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::log_deleted;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::log_user_text> {
    using NamedColumn = logging_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<255>;
    using value_type = var_string<255>;
    static constexpr NamedColumn Column = NamedColumn::log_user_text;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::log_page> {
    using NamedColumn = logging_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::log_page;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            logging_row* ptr) {
        if constexpr (NamedColumn::log_type <= Column && Column < NamedColumn::log_action) {
            return ptr->log_type;
        }
        if constexpr (NamedColumn::log_action <= Column && Column < NamedColumn::log_timestamp) {
            return ptr->log_action;
        }
        if constexpr (NamedColumn::log_timestamp <= Column && Column < NamedColumn::log_user) {
            return ptr->log_timestamp;
        }
        if constexpr (NamedColumn::log_user <= Column && Column < NamedColumn::log_namespace) {
            return ptr->log_user;
        }
        if constexpr (NamedColumn::log_namespace <= Column && Column < NamedColumn::log_title) {
            return ptr->log_namespace;
        }
        if constexpr (NamedColumn::log_title <= Column && Column < NamedColumn::log_comment) {
            return ptr->log_title;
        }
        if constexpr (NamedColumn::log_comment <= Column && Column < NamedColumn::log_params) {
            return ptr->log_comment;
        }
        if constexpr (NamedColumn::log_params <= Column && Column < NamedColumn::log_deleted) {
            return ptr->log_params;
        }
        if constexpr (NamedColumn::log_deleted <= Column && Column < NamedColumn::log_user_text) {
            return ptr->log_deleted;
        }
        if constexpr (NamedColumn::log_user_text <= Column && Column < NamedColumn::log_page) {
            return ptr->log_user_text;
        }
        if constexpr (NamedColumn::log_page <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->log_page;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 11;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(logging_row* dest, logging_row* src) {
        if constexpr(Cell == 0) {
            dest->log_type = src->log_type;
            dest->log_action = src->log_action;
            dest->log_timestamp = src->log_timestamp;
            dest->log_user = src->log_user;
            dest->log_namespace = src->log_namespace;
            dest->log_title = src->log_title;
            dest->log_comment = src->log_comment;
            dest->log_params = src->log_params;
            dest->log_deleted = src->log_deleted;
            dest->log_user_text = src->log_user_text;
            dest->log_page = src->log_page;
        }
    }
};

class RecordAccessor {
public:
    using NamedColumn = logging_row_datatypes::NamedColumn;
    //using SplitTable = logging_row_datatypes::SplitTable;
    using SplitType = logging_row_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<logging_row>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = logging_row;
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

    void copy_into(logging_row* vptr, int index=0) {
        copy_cell(index, 0, vptr, vptrs_[0]);
    }

    inline typename accessor_info<NamedColumn::log_type>::value_type& log_type() {
        return vptrs_[cell_of(NamedColumn::log_type)]->log_type;
    }

    inline const typename accessor_info<NamedColumn::log_type>::value_type& log_type() const {
        return vptrs_[cell_of(NamedColumn::log_type)]->log_type;
    }

    inline typename accessor_info<NamedColumn::log_action>::value_type& log_action() {
        return vptrs_[cell_of(NamedColumn::log_action)]->log_action;
    }

    inline const typename accessor_info<NamedColumn::log_action>::value_type& log_action() const {
        return vptrs_[cell_of(NamedColumn::log_action)]->log_action;
    }

    inline typename accessor_info<NamedColumn::log_timestamp>::value_type& log_timestamp() {
        return vptrs_[cell_of(NamedColumn::log_timestamp)]->log_timestamp;
    }

    inline const typename accessor_info<NamedColumn::log_timestamp>::value_type& log_timestamp() const {
        return vptrs_[cell_of(NamedColumn::log_timestamp)]->log_timestamp;
    }

    inline typename accessor_info<NamedColumn::log_user>::value_type& log_user() {
        return vptrs_[cell_of(NamedColumn::log_user)]->log_user;
    }

    inline const typename accessor_info<NamedColumn::log_user>::value_type& log_user() const {
        return vptrs_[cell_of(NamedColumn::log_user)]->log_user;
    }

    inline typename accessor_info<NamedColumn::log_namespace>::value_type& log_namespace() {
        return vptrs_[cell_of(NamedColumn::log_namespace)]->log_namespace;
    }

    inline const typename accessor_info<NamedColumn::log_namespace>::value_type& log_namespace() const {
        return vptrs_[cell_of(NamedColumn::log_namespace)]->log_namespace;
    }

    inline typename accessor_info<NamedColumn::log_title>::value_type& log_title() {
        return vptrs_[cell_of(NamedColumn::log_title)]->log_title;
    }

    inline const typename accessor_info<NamedColumn::log_title>::value_type& log_title() const {
        return vptrs_[cell_of(NamedColumn::log_title)]->log_title;
    }

    inline typename accessor_info<NamedColumn::log_comment>::value_type& log_comment() {
        return vptrs_[cell_of(NamedColumn::log_comment)]->log_comment;
    }

    inline const typename accessor_info<NamedColumn::log_comment>::value_type& log_comment() const {
        return vptrs_[cell_of(NamedColumn::log_comment)]->log_comment;
    }

    inline typename accessor_info<NamedColumn::log_params>::value_type& log_params() {
        return vptrs_[cell_of(NamedColumn::log_params)]->log_params;
    }

    inline const typename accessor_info<NamedColumn::log_params>::value_type& log_params() const {
        return vptrs_[cell_of(NamedColumn::log_params)]->log_params;
    }

    inline typename accessor_info<NamedColumn::log_deleted>::value_type& log_deleted() {
        return vptrs_[cell_of(NamedColumn::log_deleted)]->log_deleted;
    }

    inline const typename accessor_info<NamedColumn::log_deleted>::value_type& log_deleted() const {
        return vptrs_[cell_of(NamedColumn::log_deleted)]->log_deleted;
    }

    inline typename accessor_info<NamedColumn::log_user_text>::value_type& log_user_text() {
        return vptrs_[cell_of(NamedColumn::log_user_text)]->log_user_text;
    }

    inline const typename accessor_info<NamedColumn::log_user_text>::value_type& log_user_text() const {
        return vptrs_[cell_of(NamedColumn::log_user_text)]->log_user_text;
    }

    inline typename accessor_info<NamedColumn::log_page>::value_type& log_page() {
        return vptrs_[cell_of(NamedColumn::log_page)]->log_page;
    }

    inline const typename accessor_info<NamedColumn::log_page>::value_type& log_page() const {
        return vptrs_[cell_of(NamedColumn::log_page)]->log_page;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::log_type <= Column && Column < NamedColumn::log_action) {
            return vptrs_[cell_of(NamedColumn::log_type)]->log_type;
        }
        if constexpr (NamedColumn::log_action <= Column && Column < NamedColumn::log_timestamp) {
            return vptrs_[cell_of(NamedColumn::log_action)]->log_action;
        }
        if constexpr (NamedColumn::log_timestamp <= Column && Column < NamedColumn::log_user) {
            return vptrs_[cell_of(NamedColumn::log_timestamp)]->log_timestamp;
        }
        if constexpr (NamedColumn::log_user <= Column && Column < NamedColumn::log_namespace) {
            return vptrs_[cell_of(NamedColumn::log_user)]->log_user;
        }
        if constexpr (NamedColumn::log_namespace <= Column && Column < NamedColumn::log_title) {
            return vptrs_[cell_of(NamedColumn::log_namespace)]->log_namespace;
        }
        if constexpr (NamedColumn::log_title <= Column && Column < NamedColumn::log_comment) {
            return vptrs_[cell_of(NamedColumn::log_title)]->log_title;
        }
        if constexpr (NamedColumn::log_comment <= Column && Column < NamedColumn::log_params) {
            return vptrs_[cell_of(NamedColumn::log_comment)]->log_comment;
        }
        if constexpr (NamedColumn::log_params <= Column && Column < NamedColumn::log_deleted) {
            return vptrs_[cell_of(NamedColumn::log_params)]->log_params;
        }
        if constexpr (NamedColumn::log_deleted <= Column && Column < NamedColumn::log_user_text) {
            return vptrs_[cell_of(NamedColumn::log_deleted)]->log_deleted;
        }
        if constexpr (NamedColumn::log_user_text <= Column && Column < NamedColumn::log_page) {
            return vptrs_[cell_of(NamedColumn::log_user_text)]->log_user_text;
        }
        if constexpr (NamedColumn::log_page <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::log_page)]->log_page;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<logging_row*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace logging_row_datatypes

using logging_row = logging_row_datatypes::logging_row;

namespace page_row_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct page_row {
    using RecordAccessor = page_row_datatypes::RecordAccessor;
    using NamedColumn = page_row_datatypes::NamedColumn;

    int32_t page_namespace;
    var_string<255> page_title;
    var_string<255> page_restrictions;
    int64_t page_counter;
    float page_random;
    int32_t page_is_redirect;
    int32_t page_is_new;
    var_string<14> page_touched;
    int32_t page_latest;
    int32_t page_len;
};

enum class NamedColumn : int {
    page_namespace = 0,
    page_title,
    page_restrictions,
    page_counter,
    page_random,
    page_is_redirect,
    page_is_new,
    page_touched,
    page_latest,
    page_len,
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
    if constexpr (Column < NamedColumn::page_title) {
        return NamedColumn::page_namespace;
    }
    if constexpr (Column < NamedColumn::page_restrictions) {
        return NamedColumn::page_title;
    }
    if constexpr (Column < NamedColumn::page_counter) {
        return NamedColumn::page_restrictions;
    }
    if constexpr (Column < NamedColumn::page_random) {
        return NamedColumn::page_counter;
    }
    if constexpr (Column < NamedColumn::page_is_redirect) {
        return NamedColumn::page_random;
    }
    if constexpr (Column < NamedColumn::page_is_new) {
        return NamedColumn::page_is_redirect;
    }
    if constexpr (Column < NamedColumn::page_touched) {
        return NamedColumn::page_is_new;
    }
    if constexpr (Column < NamedColumn::page_latest) {
        return NamedColumn::page_touched;
    }
    if constexpr (Column < NamedColumn::page_len) {
        return NamedColumn::page_latest;
    }
    return NamedColumn::page_len;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::page_namespace> {
    using NamedColumn = page_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::page_namespace;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::page_title> {
    using NamedColumn = page_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<255>;
    using value_type = var_string<255>;
    static constexpr NamedColumn Column = NamedColumn::page_title;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::page_restrictions> {
    using NamedColumn = page_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<255>;
    using value_type = var_string<255>;
    static constexpr NamedColumn Column = NamedColumn::page_restrictions;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::page_counter> {
    using NamedColumn = page_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int64_t;
    using value_type = int64_t;
    static constexpr NamedColumn Column = NamedColumn::page_counter;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::page_random> {
    using NamedColumn = page_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = float;
    using value_type = float;
    static constexpr NamedColumn Column = NamedColumn::page_random;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::page_is_redirect> {
    using NamedColumn = page_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::page_is_redirect;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::page_is_new> {
    using NamedColumn = page_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::page_is_new;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::page_touched> {
    using NamedColumn = page_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<14>;
    using value_type = var_string<14>;
    static constexpr NamedColumn Column = NamedColumn::page_touched;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::page_latest> {
    using NamedColumn = page_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::page_latest;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::page_len> {
    using NamedColumn = page_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::page_len;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            page_row* ptr) {
        if constexpr (NamedColumn::page_namespace <= Column && Column < NamedColumn::page_title) {
            return ptr->page_namespace;
        }
        if constexpr (NamedColumn::page_title <= Column && Column < NamedColumn::page_restrictions) {
            return ptr->page_title;
        }
        if constexpr (NamedColumn::page_restrictions <= Column && Column < NamedColumn::page_counter) {
            return ptr->page_restrictions;
        }
        if constexpr (NamedColumn::page_counter <= Column && Column < NamedColumn::page_random) {
            return ptr->page_counter;
        }
        if constexpr (NamedColumn::page_random <= Column && Column < NamedColumn::page_is_redirect) {
            return ptr->page_random;
        }
        if constexpr (NamedColumn::page_is_redirect <= Column && Column < NamedColumn::page_is_new) {
            return ptr->page_is_redirect;
        }
        if constexpr (NamedColumn::page_is_new <= Column && Column < NamedColumn::page_touched) {
            return ptr->page_is_new;
        }
        if constexpr (NamedColumn::page_touched <= Column && Column < NamedColumn::page_latest) {
            return ptr->page_touched;
        }
        if constexpr (NamedColumn::page_latest <= Column && Column < NamedColumn::page_len) {
            return ptr->page_latest;
        }
        if constexpr (NamedColumn::page_len <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->page_len;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 10;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(page_row* dest, page_row* src) {
        if constexpr(Cell == 0) {
            dest->page_namespace = src->page_namespace;
            dest->page_title = src->page_title;
            dest->page_restrictions = src->page_restrictions;
            dest->page_counter = src->page_counter;
            dest->page_random = src->page_random;
            dest->page_is_redirect = src->page_is_redirect;
            dest->page_is_new = src->page_is_new;
            dest->page_touched = src->page_touched;
            dest->page_latest = src->page_latest;
            dest->page_len = src->page_len;
        }
    }
};

template <>
struct SplitPolicy<1> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 1) {
            return 5;
        }
        if (cell == 0) {
            return 5;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(page_row* dest, page_row* src) {
        if constexpr(Cell == 1) {
            dest->page_namespace = src->page_namespace;
            dest->page_title = src->page_title;
            dest->page_restrictions = src->page_restrictions;
            dest->page_counter = src->page_counter;
            dest->page_random = src->page_random;
        }
        if constexpr(Cell == 0) {
            dest->page_is_redirect = src->page_is_redirect;
            dest->page_is_new = src->page_is_new;
            dest->page_touched = src->page_touched;
            dest->page_latest = src->page_latest;
            dest->page_len = src->page_len;
        }
    }
};

class RecordAccessor {
public:
    using NamedColumn = page_row_datatypes::NamedColumn;
    //using SplitTable = page_row_datatypes::SplitTable;
    using SplitType = page_row_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<page_row>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = page_row;
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

    void copy_into(page_row* vptr, int index=0) {
        copy_cell(index, 0, vptr, vptrs_[0]);
        copy_cell(index, 1, vptr, vptrs_[1]);
    }

    inline typename accessor_info<NamedColumn::page_namespace>::value_type& page_namespace() {
        return vptrs_[cell_of(NamedColumn::page_namespace)]->page_namespace;
    }

    inline const typename accessor_info<NamedColumn::page_namespace>::value_type& page_namespace() const {
        return vptrs_[cell_of(NamedColumn::page_namespace)]->page_namespace;
    }

    inline typename accessor_info<NamedColumn::page_title>::value_type& page_title() {
        return vptrs_[cell_of(NamedColumn::page_title)]->page_title;
    }

    inline const typename accessor_info<NamedColumn::page_title>::value_type& page_title() const {
        return vptrs_[cell_of(NamedColumn::page_title)]->page_title;
    }

    inline typename accessor_info<NamedColumn::page_restrictions>::value_type& page_restrictions() {
        return vptrs_[cell_of(NamedColumn::page_restrictions)]->page_restrictions;
    }

    inline const typename accessor_info<NamedColumn::page_restrictions>::value_type& page_restrictions() const {
        return vptrs_[cell_of(NamedColumn::page_restrictions)]->page_restrictions;
    }

    inline typename accessor_info<NamedColumn::page_counter>::value_type& page_counter() {
        return vptrs_[cell_of(NamedColumn::page_counter)]->page_counter;
    }

    inline const typename accessor_info<NamedColumn::page_counter>::value_type& page_counter() const {
        return vptrs_[cell_of(NamedColumn::page_counter)]->page_counter;
    }

    inline typename accessor_info<NamedColumn::page_random>::value_type& page_random() {
        return vptrs_[cell_of(NamedColumn::page_random)]->page_random;
    }

    inline const typename accessor_info<NamedColumn::page_random>::value_type& page_random() const {
        return vptrs_[cell_of(NamedColumn::page_random)]->page_random;
    }

    inline typename accessor_info<NamedColumn::page_is_redirect>::value_type& page_is_redirect() {
        return vptrs_[cell_of(NamedColumn::page_is_redirect)]->page_is_redirect;
    }

    inline const typename accessor_info<NamedColumn::page_is_redirect>::value_type& page_is_redirect() const {
        return vptrs_[cell_of(NamedColumn::page_is_redirect)]->page_is_redirect;
    }

    inline typename accessor_info<NamedColumn::page_is_new>::value_type& page_is_new() {
        return vptrs_[cell_of(NamedColumn::page_is_new)]->page_is_new;
    }

    inline const typename accessor_info<NamedColumn::page_is_new>::value_type& page_is_new() const {
        return vptrs_[cell_of(NamedColumn::page_is_new)]->page_is_new;
    }

    inline typename accessor_info<NamedColumn::page_touched>::value_type& page_touched() {
        return vptrs_[cell_of(NamedColumn::page_touched)]->page_touched;
    }

    inline const typename accessor_info<NamedColumn::page_touched>::value_type& page_touched() const {
        return vptrs_[cell_of(NamedColumn::page_touched)]->page_touched;
    }

    inline typename accessor_info<NamedColumn::page_latest>::value_type& page_latest() {
        return vptrs_[cell_of(NamedColumn::page_latest)]->page_latest;
    }

    inline const typename accessor_info<NamedColumn::page_latest>::value_type& page_latest() const {
        return vptrs_[cell_of(NamedColumn::page_latest)]->page_latest;
    }

    inline typename accessor_info<NamedColumn::page_len>::value_type& page_len() {
        return vptrs_[cell_of(NamedColumn::page_len)]->page_len;
    }

    inline const typename accessor_info<NamedColumn::page_len>::value_type& page_len() const {
        return vptrs_[cell_of(NamedColumn::page_len)]->page_len;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::page_namespace <= Column && Column < NamedColumn::page_title) {
            return vptrs_[cell_of(NamedColumn::page_namespace)]->page_namespace;
        }
        if constexpr (NamedColumn::page_title <= Column && Column < NamedColumn::page_restrictions) {
            return vptrs_[cell_of(NamedColumn::page_title)]->page_title;
        }
        if constexpr (NamedColumn::page_restrictions <= Column && Column < NamedColumn::page_counter) {
            return vptrs_[cell_of(NamedColumn::page_restrictions)]->page_restrictions;
        }
        if constexpr (NamedColumn::page_counter <= Column && Column < NamedColumn::page_random) {
            return vptrs_[cell_of(NamedColumn::page_counter)]->page_counter;
        }
        if constexpr (NamedColumn::page_random <= Column && Column < NamedColumn::page_is_redirect) {
            return vptrs_[cell_of(NamedColumn::page_random)]->page_random;
        }
        if constexpr (NamedColumn::page_is_redirect <= Column && Column < NamedColumn::page_is_new) {
            return vptrs_[cell_of(NamedColumn::page_is_redirect)]->page_is_redirect;
        }
        if constexpr (NamedColumn::page_is_new <= Column && Column < NamedColumn::page_touched) {
            return vptrs_[cell_of(NamedColumn::page_is_new)]->page_is_new;
        }
        if constexpr (NamedColumn::page_touched <= Column && Column < NamedColumn::page_latest) {
            return vptrs_[cell_of(NamedColumn::page_touched)]->page_touched;
        }
        if constexpr (NamedColumn::page_latest <= Column && Column < NamedColumn::page_len) {
            return vptrs_[cell_of(NamedColumn::page_latest)]->page_latest;
        }
        if constexpr (NamedColumn::page_len <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::page_len)]->page_len;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<page_row*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace page_row_datatypes

using page_row = page_row_datatypes::page_row;

namespace page_idx_row_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct page_idx_row {
    using RecordAccessor = page_idx_row_datatypes::RecordAccessor;
    using NamedColumn = page_idx_row_datatypes::NamedColumn;

    int32_t page_id;
};

enum class NamedColumn : int {
    page_id = 0,
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
    return NamedColumn::page_id;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::page_id> {
    using NamedColumn = page_idx_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::page_id;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            page_idx_row* ptr) {
        if constexpr (NamedColumn::page_id <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->page_id;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 1;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(page_idx_row* dest, page_idx_row* src) {
        if constexpr(Cell == 0) {
            dest->page_id = src->page_id;
        }
    }
};

class RecordAccessor {
public:
    using NamedColumn = page_idx_row_datatypes::NamedColumn;
    //using SplitTable = page_idx_row_datatypes::SplitTable;
    using SplitType = page_idx_row_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<page_idx_row>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = page_idx_row;
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

    void copy_into(page_idx_row* vptr, int index=0) {
        copy_cell(index, 0, vptr, vptrs_[0]);
    }

    inline typename accessor_info<NamedColumn::page_id>::value_type& page_id() {
        return vptrs_[cell_of(NamedColumn::page_id)]->page_id;
    }

    inline const typename accessor_info<NamedColumn::page_id>::value_type& page_id() const {
        return vptrs_[cell_of(NamedColumn::page_id)]->page_id;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::page_id <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::page_id)]->page_id;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<page_idx_row*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace page_idx_row_datatypes

using page_idx_row = page_idx_row_datatypes::page_idx_row;

namespace page_restrictions_row_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct page_restrictions_row {
    using RecordAccessor = page_restrictions_row_datatypes::RecordAccessor;
    using NamedColumn = page_restrictions_row_datatypes::NamedColumn;

    int32_t pr_page;
    var_string<60> pr_type;
    var_string<60> pr_level;
    int32_t pr_cascade;
    int32_t pr_user;
    var_string<14> pr_expiry;
};

enum class NamedColumn : int {
    pr_page = 0,
    pr_type,
    pr_level,
    pr_cascade,
    pr_user,
    pr_expiry,
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
    if constexpr (Column < NamedColumn::pr_type) {
        return NamedColumn::pr_page;
    }
    if constexpr (Column < NamedColumn::pr_level) {
        return NamedColumn::pr_type;
    }
    if constexpr (Column < NamedColumn::pr_cascade) {
        return NamedColumn::pr_level;
    }
    if constexpr (Column < NamedColumn::pr_user) {
        return NamedColumn::pr_cascade;
    }
    if constexpr (Column < NamedColumn::pr_expiry) {
        return NamedColumn::pr_user;
    }
    return NamedColumn::pr_expiry;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::pr_page> {
    using NamedColumn = page_restrictions_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::pr_page;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::pr_type> {
    using NamedColumn = page_restrictions_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<60>;
    using value_type = var_string<60>;
    static constexpr NamedColumn Column = NamedColumn::pr_type;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::pr_level> {
    using NamedColumn = page_restrictions_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<60>;
    using value_type = var_string<60>;
    static constexpr NamedColumn Column = NamedColumn::pr_level;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::pr_cascade> {
    using NamedColumn = page_restrictions_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::pr_cascade;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::pr_user> {
    using NamedColumn = page_restrictions_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::pr_user;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::pr_expiry> {
    using NamedColumn = page_restrictions_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<14>;
    using value_type = var_string<14>;
    static constexpr NamedColumn Column = NamedColumn::pr_expiry;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            page_restrictions_row* ptr) {
        if constexpr (NamedColumn::pr_page <= Column && Column < NamedColumn::pr_type) {
            return ptr->pr_page;
        }
        if constexpr (NamedColumn::pr_type <= Column && Column < NamedColumn::pr_level) {
            return ptr->pr_type;
        }
        if constexpr (NamedColumn::pr_level <= Column && Column < NamedColumn::pr_cascade) {
            return ptr->pr_level;
        }
        if constexpr (NamedColumn::pr_cascade <= Column && Column < NamedColumn::pr_user) {
            return ptr->pr_cascade;
        }
        if constexpr (NamedColumn::pr_user <= Column && Column < NamedColumn::pr_expiry) {
            return ptr->pr_user;
        }
        if constexpr (NamedColumn::pr_expiry <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->pr_expiry;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 6;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(page_restrictions_row* dest, page_restrictions_row* src) {
        if constexpr(Cell == 0) {
            dest->pr_page = src->pr_page;
            dest->pr_type = src->pr_type;
            dest->pr_level = src->pr_level;
            dest->pr_cascade = src->pr_cascade;
            dest->pr_user = src->pr_user;
            dest->pr_expiry = src->pr_expiry;
        }
    }
};

class RecordAccessor {
public:
    using NamedColumn = page_restrictions_row_datatypes::NamedColumn;
    //using SplitTable = page_restrictions_row_datatypes::SplitTable;
    using SplitType = page_restrictions_row_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<page_restrictions_row>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = page_restrictions_row;
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

    void copy_into(page_restrictions_row* vptr, int index=0) {
        copy_cell(index, 0, vptr, vptrs_[0]);
    }

    inline typename accessor_info<NamedColumn::pr_page>::value_type& pr_page() {
        return vptrs_[cell_of(NamedColumn::pr_page)]->pr_page;
    }

    inline const typename accessor_info<NamedColumn::pr_page>::value_type& pr_page() const {
        return vptrs_[cell_of(NamedColumn::pr_page)]->pr_page;
    }

    inline typename accessor_info<NamedColumn::pr_type>::value_type& pr_type() {
        return vptrs_[cell_of(NamedColumn::pr_type)]->pr_type;
    }

    inline const typename accessor_info<NamedColumn::pr_type>::value_type& pr_type() const {
        return vptrs_[cell_of(NamedColumn::pr_type)]->pr_type;
    }

    inline typename accessor_info<NamedColumn::pr_level>::value_type& pr_level() {
        return vptrs_[cell_of(NamedColumn::pr_level)]->pr_level;
    }

    inline const typename accessor_info<NamedColumn::pr_level>::value_type& pr_level() const {
        return vptrs_[cell_of(NamedColumn::pr_level)]->pr_level;
    }

    inline typename accessor_info<NamedColumn::pr_cascade>::value_type& pr_cascade() {
        return vptrs_[cell_of(NamedColumn::pr_cascade)]->pr_cascade;
    }

    inline const typename accessor_info<NamedColumn::pr_cascade>::value_type& pr_cascade() const {
        return vptrs_[cell_of(NamedColumn::pr_cascade)]->pr_cascade;
    }

    inline typename accessor_info<NamedColumn::pr_user>::value_type& pr_user() {
        return vptrs_[cell_of(NamedColumn::pr_user)]->pr_user;
    }

    inline const typename accessor_info<NamedColumn::pr_user>::value_type& pr_user() const {
        return vptrs_[cell_of(NamedColumn::pr_user)]->pr_user;
    }

    inline typename accessor_info<NamedColumn::pr_expiry>::value_type& pr_expiry() {
        return vptrs_[cell_of(NamedColumn::pr_expiry)]->pr_expiry;
    }

    inline const typename accessor_info<NamedColumn::pr_expiry>::value_type& pr_expiry() const {
        return vptrs_[cell_of(NamedColumn::pr_expiry)]->pr_expiry;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::pr_page <= Column && Column < NamedColumn::pr_type) {
            return vptrs_[cell_of(NamedColumn::pr_page)]->pr_page;
        }
        if constexpr (NamedColumn::pr_type <= Column && Column < NamedColumn::pr_level) {
            return vptrs_[cell_of(NamedColumn::pr_type)]->pr_type;
        }
        if constexpr (NamedColumn::pr_level <= Column && Column < NamedColumn::pr_cascade) {
            return vptrs_[cell_of(NamedColumn::pr_level)]->pr_level;
        }
        if constexpr (NamedColumn::pr_cascade <= Column && Column < NamedColumn::pr_user) {
            return vptrs_[cell_of(NamedColumn::pr_cascade)]->pr_cascade;
        }
        if constexpr (NamedColumn::pr_user <= Column && Column < NamedColumn::pr_expiry) {
            return vptrs_[cell_of(NamedColumn::pr_user)]->pr_user;
        }
        if constexpr (NamedColumn::pr_expiry <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::pr_expiry)]->pr_expiry;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<page_restrictions_row*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace page_restrictions_row_datatypes

using page_restrictions_row = page_restrictions_row_datatypes::page_restrictions_row;

namespace page_restrictions_idx_row_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct page_restrictions_idx_row {
    using RecordAccessor = page_restrictions_idx_row_datatypes::RecordAccessor;
    using NamedColumn = page_restrictions_idx_row_datatypes::NamedColumn;

    int32_t pr_id;
};

enum class NamedColumn : int {
    pr_id = 0,
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
    return NamedColumn::pr_id;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::pr_id> {
    using NamedColumn = page_restrictions_idx_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::pr_id;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            page_restrictions_idx_row* ptr) {
        if constexpr (NamedColumn::pr_id <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->pr_id;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 1;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(page_restrictions_idx_row* dest, page_restrictions_idx_row* src) {
        if constexpr(Cell == 0) {
            dest->pr_id = src->pr_id;
        }
    }
};

class RecordAccessor {
public:
    using NamedColumn = page_restrictions_idx_row_datatypes::NamedColumn;
    //using SplitTable = page_restrictions_idx_row_datatypes::SplitTable;
    using SplitType = page_restrictions_idx_row_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<page_restrictions_idx_row>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = page_restrictions_idx_row;
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

    void copy_into(page_restrictions_idx_row* vptr, int index=0) {
        copy_cell(index, 0, vptr, vptrs_[0]);
    }

    inline typename accessor_info<NamedColumn::pr_id>::value_type& pr_id() {
        return vptrs_[cell_of(NamedColumn::pr_id)]->pr_id;
    }

    inline const typename accessor_info<NamedColumn::pr_id>::value_type& pr_id() const {
        return vptrs_[cell_of(NamedColumn::pr_id)]->pr_id;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::pr_id <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::pr_id)]->pr_id;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<page_restrictions_idx_row*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace page_restrictions_idx_row_datatypes

using page_restrictions_idx_row = page_restrictions_idx_row_datatypes::page_restrictions_idx_row;

namespace recentchanges_row_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct recentchanges_row {
    using RecordAccessor = recentchanges_row_datatypes::RecordAccessor;
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;

    var_string<14> rc_timestamp;
    var_string<14> rc_cur_time;
    int32_t rc_user;
    var_string<255> rc_user_text;
    int32_t rc_namespace;
    var_string<255> rc_title;
    var_string<255> rc_comment;
    int32_t rc_minor;
    int32_t rc_bot;
    int32_t rc_new;
    int32_t rc_cur_id;
    int32_t rc_this_oldid;
    int32_t rc_last_oldid;
    int32_t rc_type;
    int32_t rc_moved_to_ns;
    var_string<255> rc_moved_to_title;
    int32_t rc_patrolled;
    var_string<40> rc_ip;
    int32_t rc_old_len;
    int32_t rc_new_len;
    int32_t rc_deleted;
    int32_t rc_logid;
    var_string<255> rc_log_type;
    var_string<255> rc_log_action;
    var_string<255> rc_params;
};

enum class NamedColumn : int {
    rc_timestamp = 0,
    rc_cur_time,
    rc_user,
    rc_user_text,
    rc_namespace,
    rc_title,
    rc_comment,
    rc_minor,
    rc_bot,
    rc_new,
    rc_cur_id,
    rc_this_oldid,
    rc_last_oldid,
    rc_type,
    rc_moved_to_ns,
    rc_moved_to_title,
    rc_patrolled,
    rc_ip,
    rc_old_len,
    rc_new_len,
    rc_deleted,
    rc_logid,
    rc_log_type,
    rc_log_action,
    rc_params,
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
    if constexpr (Column < NamedColumn::rc_cur_time) {
        return NamedColumn::rc_timestamp;
    }
    if constexpr (Column < NamedColumn::rc_user) {
        return NamedColumn::rc_cur_time;
    }
    if constexpr (Column < NamedColumn::rc_user_text) {
        return NamedColumn::rc_user;
    }
    if constexpr (Column < NamedColumn::rc_namespace) {
        return NamedColumn::rc_user_text;
    }
    if constexpr (Column < NamedColumn::rc_title) {
        return NamedColumn::rc_namespace;
    }
    if constexpr (Column < NamedColumn::rc_comment) {
        return NamedColumn::rc_title;
    }
    if constexpr (Column < NamedColumn::rc_minor) {
        return NamedColumn::rc_comment;
    }
    if constexpr (Column < NamedColumn::rc_bot) {
        return NamedColumn::rc_minor;
    }
    if constexpr (Column < NamedColumn::rc_new) {
        return NamedColumn::rc_bot;
    }
    if constexpr (Column < NamedColumn::rc_cur_id) {
        return NamedColumn::rc_new;
    }
    if constexpr (Column < NamedColumn::rc_this_oldid) {
        return NamedColumn::rc_cur_id;
    }
    if constexpr (Column < NamedColumn::rc_last_oldid) {
        return NamedColumn::rc_this_oldid;
    }
    if constexpr (Column < NamedColumn::rc_type) {
        return NamedColumn::rc_last_oldid;
    }
    if constexpr (Column < NamedColumn::rc_moved_to_ns) {
        return NamedColumn::rc_type;
    }
    if constexpr (Column < NamedColumn::rc_moved_to_title) {
        return NamedColumn::rc_moved_to_ns;
    }
    if constexpr (Column < NamedColumn::rc_patrolled) {
        return NamedColumn::rc_moved_to_title;
    }
    if constexpr (Column < NamedColumn::rc_ip) {
        return NamedColumn::rc_patrolled;
    }
    if constexpr (Column < NamedColumn::rc_old_len) {
        return NamedColumn::rc_ip;
    }
    if constexpr (Column < NamedColumn::rc_new_len) {
        return NamedColumn::rc_old_len;
    }
    if constexpr (Column < NamedColumn::rc_deleted) {
        return NamedColumn::rc_new_len;
    }
    if constexpr (Column < NamedColumn::rc_logid) {
        return NamedColumn::rc_deleted;
    }
    if constexpr (Column < NamedColumn::rc_log_type) {
        return NamedColumn::rc_logid;
    }
    if constexpr (Column < NamedColumn::rc_log_action) {
        return NamedColumn::rc_log_type;
    }
    if constexpr (Column < NamedColumn::rc_params) {
        return NamedColumn::rc_log_action;
    }
    return NamedColumn::rc_params;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::rc_timestamp> {
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<14>;
    using value_type = var_string<14>;
    static constexpr NamedColumn Column = NamedColumn::rc_timestamp;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rc_cur_time> {
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<14>;
    using value_type = var_string<14>;
    static constexpr NamedColumn Column = NamedColumn::rc_cur_time;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rc_user> {
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::rc_user;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rc_user_text> {
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<255>;
    using value_type = var_string<255>;
    static constexpr NamedColumn Column = NamedColumn::rc_user_text;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rc_namespace> {
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::rc_namespace;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rc_title> {
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<255>;
    using value_type = var_string<255>;
    static constexpr NamedColumn Column = NamedColumn::rc_title;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rc_comment> {
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<255>;
    using value_type = var_string<255>;
    static constexpr NamedColumn Column = NamedColumn::rc_comment;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rc_minor> {
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::rc_minor;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rc_bot> {
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::rc_bot;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rc_new> {
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::rc_new;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rc_cur_id> {
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::rc_cur_id;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rc_this_oldid> {
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::rc_this_oldid;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rc_last_oldid> {
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::rc_last_oldid;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rc_type> {
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::rc_type;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rc_moved_to_ns> {
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::rc_moved_to_ns;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rc_moved_to_title> {
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<255>;
    using value_type = var_string<255>;
    static constexpr NamedColumn Column = NamedColumn::rc_moved_to_title;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rc_patrolled> {
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::rc_patrolled;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rc_ip> {
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<40>;
    using value_type = var_string<40>;
    static constexpr NamedColumn Column = NamedColumn::rc_ip;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rc_old_len> {
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::rc_old_len;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rc_new_len> {
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::rc_new_len;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rc_deleted> {
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::rc_deleted;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rc_logid> {
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::rc_logid;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rc_log_type> {
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<255>;
    using value_type = var_string<255>;
    static constexpr NamedColumn Column = NamedColumn::rc_log_type;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rc_log_action> {
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<255>;
    using value_type = var_string<255>;
    static constexpr NamedColumn Column = NamedColumn::rc_log_action;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rc_params> {
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<255>;
    using value_type = var_string<255>;
    static constexpr NamedColumn Column = NamedColumn::rc_params;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            recentchanges_row* ptr) {
        if constexpr (NamedColumn::rc_timestamp <= Column && Column < NamedColumn::rc_cur_time) {
            return ptr->rc_timestamp;
        }
        if constexpr (NamedColumn::rc_cur_time <= Column && Column < NamedColumn::rc_user) {
            return ptr->rc_cur_time;
        }
        if constexpr (NamedColumn::rc_user <= Column && Column < NamedColumn::rc_user_text) {
            return ptr->rc_user;
        }
        if constexpr (NamedColumn::rc_user_text <= Column && Column < NamedColumn::rc_namespace) {
            return ptr->rc_user_text;
        }
        if constexpr (NamedColumn::rc_namespace <= Column && Column < NamedColumn::rc_title) {
            return ptr->rc_namespace;
        }
        if constexpr (NamedColumn::rc_title <= Column && Column < NamedColumn::rc_comment) {
            return ptr->rc_title;
        }
        if constexpr (NamedColumn::rc_comment <= Column && Column < NamedColumn::rc_minor) {
            return ptr->rc_comment;
        }
        if constexpr (NamedColumn::rc_minor <= Column && Column < NamedColumn::rc_bot) {
            return ptr->rc_minor;
        }
        if constexpr (NamedColumn::rc_bot <= Column && Column < NamedColumn::rc_new) {
            return ptr->rc_bot;
        }
        if constexpr (NamedColumn::rc_new <= Column && Column < NamedColumn::rc_cur_id) {
            return ptr->rc_new;
        }
        if constexpr (NamedColumn::rc_cur_id <= Column && Column < NamedColumn::rc_this_oldid) {
            return ptr->rc_cur_id;
        }
        if constexpr (NamedColumn::rc_this_oldid <= Column && Column < NamedColumn::rc_last_oldid) {
            return ptr->rc_this_oldid;
        }
        if constexpr (NamedColumn::rc_last_oldid <= Column && Column < NamedColumn::rc_type) {
            return ptr->rc_last_oldid;
        }
        if constexpr (NamedColumn::rc_type <= Column && Column < NamedColumn::rc_moved_to_ns) {
            return ptr->rc_type;
        }
        if constexpr (NamedColumn::rc_moved_to_ns <= Column && Column < NamedColumn::rc_moved_to_title) {
            return ptr->rc_moved_to_ns;
        }
        if constexpr (NamedColumn::rc_moved_to_title <= Column && Column < NamedColumn::rc_patrolled) {
            return ptr->rc_moved_to_title;
        }
        if constexpr (NamedColumn::rc_patrolled <= Column && Column < NamedColumn::rc_ip) {
            return ptr->rc_patrolled;
        }
        if constexpr (NamedColumn::rc_ip <= Column && Column < NamedColumn::rc_old_len) {
            return ptr->rc_ip;
        }
        if constexpr (NamedColumn::rc_old_len <= Column && Column < NamedColumn::rc_new_len) {
            return ptr->rc_old_len;
        }
        if constexpr (NamedColumn::rc_new_len <= Column && Column < NamedColumn::rc_deleted) {
            return ptr->rc_new_len;
        }
        if constexpr (NamedColumn::rc_deleted <= Column && Column < NamedColumn::rc_logid) {
            return ptr->rc_deleted;
        }
        if constexpr (NamedColumn::rc_logid <= Column && Column < NamedColumn::rc_log_type) {
            return ptr->rc_logid;
        }
        if constexpr (NamedColumn::rc_log_type <= Column && Column < NamedColumn::rc_log_action) {
            return ptr->rc_log_type;
        }
        if constexpr (NamedColumn::rc_log_action <= Column && Column < NamedColumn::rc_params) {
            return ptr->rc_log_action;
        }
        if constexpr (NamedColumn::rc_params <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->rc_params;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 25;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(recentchanges_row* dest, recentchanges_row* src) {
        if constexpr(Cell == 0) {
            dest->rc_timestamp = src->rc_timestamp;
            dest->rc_cur_time = src->rc_cur_time;
            dest->rc_user = src->rc_user;
            dest->rc_user_text = src->rc_user_text;
            dest->rc_namespace = src->rc_namespace;
            dest->rc_title = src->rc_title;
            dest->rc_comment = src->rc_comment;
            dest->rc_minor = src->rc_minor;
            dest->rc_bot = src->rc_bot;
            dest->rc_new = src->rc_new;
            dest->rc_cur_id = src->rc_cur_id;
            dest->rc_this_oldid = src->rc_this_oldid;
            dest->rc_last_oldid = src->rc_last_oldid;
            dest->rc_type = src->rc_type;
            dest->rc_moved_to_ns = src->rc_moved_to_ns;
            dest->rc_moved_to_title = src->rc_moved_to_title;
            dest->rc_patrolled = src->rc_patrolled;
            dest->rc_ip = src->rc_ip;
            dest->rc_old_len = src->rc_old_len;
            dest->rc_new_len = src->rc_new_len;
            dest->rc_deleted = src->rc_deleted;
            dest->rc_logid = src->rc_logid;
            dest->rc_log_type = src->rc_log_type;
            dest->rc_log_action = src->rc_log_action;
            dest->rc_params = src->rc_params;
        }
    }
};

class RecordAccessor {
public:
    using NamedColumn = recentchanges_row_datatypes::NamedColumn;
    //using SplitTable = recentchanges_row_datatypes::SplitTable;
    using SplitType = recentchanges_row_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<recentchanges_row>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = recentchanges_row;
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

    void copy_into(recentchanges_row* vptr, int index=0) {
        copy_cell(index, 0, vptr, vptrs_[0]);
    }

    inline typename accessor_info<NamedColumn::rc_timestamp>::value_type& rc_timestamp() {
        return vptrs_[cell_of(NamedColumn::rc_timestamp)]->rc_timestamp;
    }

    inline const typename accessor_info<NamedColumn::rc_timestamp>::value_type& rc_timestamp() const {
        return vptrs_[cell_of(NamedColumn::rc_timestamp)]->rc_timestamp;
    }

    inline typename accessor_info<NamedColumn::rc_cur_time>::value_type& rc_cur_time() {
        return vptrs_[cell_of(NamedColumn::rc_cur_time)]->rc_cur_time;
    }

    inline const typename accessor_info<NamedColumn::rc_cur_time>::value_type& rc_cur_time() const {
        return vptrs_[cell_of(NamedColumn::rc_cur_time)]->rc_cur_time;
    }

    inline typename accessor_info<NamedColumn::rc_user>::value_type& rc_user() {
        return vptrs_[cell_of(NamedColumn::rc_user)]->rc_user;
    }

    inline const typename accessor_info<NamedColumn::rc_user>::value_type& rc_user() const {
        return vptrs_[cell_of(NamedColumn::rc_user)]->rc_user;
    }

    inline typename accessor_info<NamedColumn::rc_user_text>::value_type& rc_user_text() {
        return vptrs_[cell_of(NamedColumn::rc_user_text)]->rc_user_text;
    }

    inline const typename accessor_info<NamedColumn::rc_user_text>::value_type& rc_user_text() const {
        return vptrs_[cell_of(NamedColumn::rc_user_text)]->rc_user_text;
    }

    inline typename accessor_info<NamedColumn::rc_namespace>::value_type& rc_namespace() {
        return vptrs_[cell_of(NamedColumn::rc_namespace)]->rc_namespace;
    }

    inline const typename accessor_info<NamedColumn::rc_namespace>::value_type& rc_namespace() const {
        return vptrs_[cell_of(NamedColumn::rc_namespace)]->rc_namespace;
    }

    inline typename accessor_info<NamedColumn::rc_title>::value_type& rc_title() {
        return vptrs_[cell_of(NamedColumn::rc_title)]->rc_title;
    }

    inline const typename accessor_info<NamedColumn::rc_title>::value_type& rc_title() const {
        return vptrs_[cell_of(NamedColumn::rc_title)]->rc_title;
    }

    inline typename accessor_info<NamedColumn::rc_comment>::value_type& rc_comment() {
        return vptrs_[cell_of(NamedColumn::rc_comment)]->rc_comment;
    }

    inline const typename accessor_info<NamedColumn::rc_comment>::value_type& rc_comment() const {
        return vptrs_[cell_of(NamedColumn::rc_comment)]->rc_comment;
    }

    inline typename accessor_info<NamedColumn::rc_minor>::value_type& rc_minor() {
        return vptrs_[cell_of(NamedColumn::rc_minor)]->rc_minor;
    }

    inline const typename accessor_info<NamedColumn::rc_minor>::value_type& rc_minor() const {
        return vptrs_[cell_of(NamedColumn::rc_minor)]->rc_minor;
    }

    inline typename accessor_info<NamedColumn::rc_bot>::value_type& rc_bot() {
        return vptrs_[cell_of(NamedColumn::rc_bot)]->rc_bot;
    }

    inline const typename accessor_info<NamedColumn::rc_bot>::value_type& rc_bot() const {
        return vptrs_[cell_of(NamedColumn::rc_bot)]->rc_bot;
    }

    inline typename accessor_info<NamedColumn::rc_new>::value_type& rc_new() {
        return vptrs_[cell_of(NamedColumn::rc_new)]->rc_new;
    }

    inline const typename accessor_info<NamedColumn::rc_new>::value_type& rc_new() const {
        return vptrs_[cell_of(NamedColumn::rc_new)]->rc_new;
    }

    inline typename accessor_info<NamedColumn::rc_cur_id>::value_type& rc_cur_id() {
        return vptrs_[cell_of(NamedColumn::rc_cur_id)]->rc_cur_id;
    }

    inline const typename accessor_info<NamedColumn::rc_cur_id>::value_type& rc_cur_id() const {
        return vptrs_[cell_of(NamedColumn::rc_cur_id)]->rc_cur_id;
    }

    inline typename accessor_info<NamedColumn::rc_this_oldid>::value_type& rc_this_oldid() {
        return vptrs_[cell_of(NamedColumn::rc_this_oldid)]->rc_this_oldid;
    }

    inline const typename accessor_info<NamedColumn::rc_this_oldid>::value_type& rc_this_oldid() const {
        return vptrs_[cell_of(NamedColumn::rc_this_oldid)]->rc_this_oldid;
    }

    inline typename accessor_info<NamedColumn::rc_last_oldid>::value_type& rc_last_oldid() {
        return vptrs_[cell_of(NamedColumn::rc_last_oldid)]->rc_last_oldid;
    }

    inline const typename accessor_info<NamedColumn::rc_last_oldid>::value_type& rc_last_oldid() const {
        return vptrs_[cell_of(NamedColumn::rc_last_oldid)]->rc_last_oldid;
    }

    inline typename accessor_info<NamedColumn::rc_type>::value_type& rc_type() {
        return vptrs_[cell_of(NamedColumn::rc_type)]->rc_type;
    }

    inline const typename accessor_info<NamedColumn::rc_type>::value_type& rc_type() const {
        return vptrs_[cell_of(NamedColumn::rc_type)]->rc_type;
    }

    inline typename accessor_info<NamedColumn::rc_moved_to_ns>::value_type& rc_moved_to_ns() {
        return vptrs_[cell_of(NamedColumn::rc_moved_to_ns)]->rc_moved_to_ns;
    }

    inline const typename accessor_info<NamedColumn::rc_moved_to_ns>::value_type& rc_moved_to_ns() const {
        return vptrs_[cell_of(NamedColumn::rc_moved_to_ns)]->rc_moved_to_ns;
    }

    inline typename accessor_info<NamedColumn::rc_moved_to_title>::value_type& rc_moved_to_title() {
        return vptrs_[cell_of(NamedColumn::rc_moved_to_title)]->rc_moved_to_title;
    }

    inline const typename accessor_info<NamedColumn::rc_moved_to_title>::value_type& rc_moved_to_title() const {
        return vptrs_[cell_of(NamedColumn::rc_moved_to_title)]->rc_moved_to_title;
    }

    inline typename accessor_info<NamedColumn::rc_patrolled>::value_type& rc_patrolled() {
        return vptrs_[cell_of(NamedColumn::rc_patrolled)]->rc_patrolled;
    }

    inline const typename accessor_info<NamedColumn::rc_patrolled>::value_type& rc_patrolled() const {
        return vptrs_[cell_of(NamedColumn::rc_patrolled)]->rc_patrolled;
    }

    inline typename accessor_info<NamedColumn::rc_ip>::value_type& rc_ip() {
        return vptrs_[cell_of(NamedColumn::rc_ip)]->rc_ip;
    }

    inline const typename accessor_info<NamedColumn::rc_ip>::value_type& rc_ip() const {
        return vptrs_[cell_of(NamedColumn::rc_ip)]->rc_ip;
    }

    inline typename accessor_info<NamedColumn::rc_old_len>::value_type& rc_old_len() {
        return vptrs_[cell_of(NamedColumn::rc_old_len)]->rc_old_len;
    }

    inline const typename accessor_info<NamedColumn::rc_old_len>::value_type& rc_old_len() const {
        return vptrs_[cell_of(NamedColumn::rc_old_len)]->rc_old_len;
    }

    inline typename accessor_info<NamedColumn::rc_new_len>::value_type& rc_new_len() {
        return vptrs_[cell_of(NamedColumn::rc_new_len)]->rc_new_len;
    }

    inline const typename accessor_info<NamedColumn::rc_new_len>::value_type& rc_new_len() const {
        return vptrs_[cell_of(NamedColumn::rc_new_len)]->rc_new_len;
    }

    inline typename accessor_info<NamedColumn::rc_deleted>::value_type& rc_deleted() {
        return vptrs_[cell_of(NamedColumn::rc_deleted)]->rc_deleted;
    }

    inline const typename accessor_info<NamedColumn::rc_deleted>::value_type& rc_deleted() const {
        return vptrs_[cell_of(NamedColumn::rc_deleted)]->rc_deleted;
    }

    inline typename accessor_info<NamedColumn::rc_logid>::value_type& rc_logid() {
        return vptrs_[cell_of(NamedColumn::rc_logid)]->rc_logid;
    }

    inline const typename accessor_info<NamedColumn::rc_logid>::value_type& rc_logid() const {
        return vptrs_[cell_of(NamedColumn::rc_logid)]->rc_logid;
    }

    inline typename accessor_info<NamedColumn::rc_log_type>::value_type& rc_log_type() {
        return vptrs_[cell_of(NamedColumn::rc_log_type)]->rc_log_type;
    }

    inline const typename accessor_info<NamedColumn::rc_log_type>::value_type& rc_log_type() const {
        return vptrs_[cell_of(NamedColumn::rc_log_type)]->rc_log_type;
    }

    inline typename accessor_info<NamedColumn::rc_log_action>::value_type& rc_log_action() {
        return vptrs_[cell_of(NamedColumn::rc_log_action)]->rc_log_action;
    }

    inline const typename accessor_info<NamedColumn::rc_log_action>::value_type& rc_log_action() const {
        return vptrs_[cell_of(NamedColumn::rc_log_action)]->rc_log_action;
    }

    inline typename accessor_info<NamedColumn::rc_params>::value_type& rc_params() {
        return vptrs_[cell_of(NamedColumn::rc_params)]->rc_params;
    }

    inline const typename accessor_info<NamedColumn::rc_params>::value_type& rc_params() const {
        return vptrs_[cell_of(NamedColumn::rc_params)]->rc_params;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::rc_timestamp <= Column && Column < NamedColumn::rc_cur_time) {
            return vptrs_[cell_of(NamedColumn::rc_timestamp)]->rc_timestamp;
        }
        if constexpr (NamedColumn::rc_cur_time <= Column && Column < NamedColumn::rc_user) {
            return vptrs_[cell_of(NamedColumn::rc_cur_time)]->rc_cur_time;
        }
        if constexpr (NamedColumn::rc_user <= Column && Column < NamedColumn::rc_user_text) {
            return vptrs_[cell_of(NamedColumn::rc_user)]->rc_user;
        }
        if constexpr (NamedColumn::rc_user_text <= Column && Column < NamedColumn::rc_namespace) {
            return vptrs_[cell_of(NamedColumn::rc_user_text)]->rc_user_text;
        }
        if constexpr (NamedColumn::rc_namespace <= Column && Column < NamedColumn::rc_title) {
            return vptrs_[cell_of(NamedColumn::rc_namespace)]->rc_namespace;
        }
        if constexpr (NamedColumn::rc_title <= Column && Column < NamedColumn::rc_comment) {
            return vptrs_[cell_of(NamedColumn::rc_title)]->rc_title;
        }
        if constexpr (NamedColumn::rc_comment <= Column && Column < NamedColumn::rc_minor) {
            return vptrs_[cell_of(NamedColumn::rc_comment)]->rc_comment;
        }
        if constexpr (NamedColumn::rc_minor <= Column && Column < NamedColumn::rc_bot) {
            return vptrs_[cell_of(NamedColumn::rc_minor)]->rc_minor;
        }
        if constexpr (NamedColumn::rc_bot <= Column && Column < NamedColumn::rc_new) {
            return vptrs_[cell_of(NamedColumn::rc_bot)]->rc_bot;
        }
        if constexpr (NamedColumn::rc_new <= Column && Column < NamedColumn::rc_cur_id) {
            return vptrs_[cell_of(NamedColumn::rc_new)]->rc_new;
        }
        if constexpr (NamedColumn::rc_cur_id <= Column && Column < NamedColumn::rc_this_oldid) {
            return vptrs_[cell_of(NamedColumn::rc_cur_id)]->rc_cur_id;
        }
        if constexpr (NamedColumn::rc_this_oldid <= Column && Column < NamedColumn::rc_last_oldid) {
            return vptrs_[cell_of(NamedColumn::rc_this_oldid)]->rc_this_oldid;
        }
        if constexpr (NamedColumn::rc_last_oldid <= Column && Column < NamedColumn::rc_type) {
            return vptrs_[cell_of(NamedColumn::rc_last_oldid)]->rc_last_oldid;
        }
        if constexpr (NamedColumn::rc_type <= Column && Column < NamedColumn::rc_moved_to_ns) {
            return vptrs_[cell_of(NamedColumn::rc_type)]->rc_type;
        }
        if constexpr (NamedColumn::rc_moved_to_ns <= Column && Column < NamedColumn::rc_moved_to_title) {
            return vptrs_[cell_of(NamedColumn::rc_moved_to_ns)]->rc_moved_to_ns;
        }
        if constexpr (NamedColumn::rc_moved_to_title <= Column && Column < NamedColumn::rc_patrolled) {
            return vptrs_[cell_of(NamedColumn::rc_moved_to_title)]->rc_moved_to_title;
        }
        if constexpr (NamedColumn::rc_patrolled <= Column && Column < NamedColumn::rc_ip) {
            return vptrs_[cell_of(NamedColumn::rc_patrolled)]->rc_patrolled;
        }
        if constexpr (NamedColumn::rc_ip <= Column && Column < NamedColumn::rc_old_len) {
            return vptrs_[cell_of(NamedColumn::rc_ip)]->rc_ip;
        }
        if constexpr (NamedColumn::rc_old_len <= Column && Column < NamedColumn::rc_new_len) {
            return vptrs_[cell_of(NamedColumn::rc_old_len)]->rc_old_len;
        }
        if constexpr (NamedColumn::rc_new_len <= Column && Column < NamedColumn::rc_deleted) {
            return vptrs_[cell_of(NamedColumn::rc_new_len)]->rc_new_len;
        }
        if constexpr (NamedColumn::rc_deleted <= Column && Column < NamedColumn::rc_logid) {
            return vptrs_[cell_of(NamedColumn::rc_deleted)]->rc_deleted;
        }
        if constexpr (NamedColumn::rc_logid <= Column && Column < NamedColumn::rc_log_type) {
            return vptrs_[cell_of(NamedColumn::rc_logid)]->rc_logid;
        }
        if constexpr (NamedColumn::rc_log_type <= Column && Column < NamedColumn::rc_log_action) {
            return vptrs_[cell_of(NamedColumn::rc_log_type)]->rc_log_type;
        }
        if constexpr (NamedColumn::rc_log_action <= Column && Column < NamedColumn::rc_params) {
            return vptrs_[cell_of(NamedColumn::rc_log_action)]->rc_log_action;
        }
        if constexpr (NamedColumn::rc_params <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::rc_params)]->rc_params;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<recentchanges_row*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace recentchanges_row_datatypes

using recentchanges_row = recentchanges_row_datatypes::recentchanges_row;

namespace revision_row_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct revision_row {
    using RecordAccessor = revision_row_datatypes::RecordAccessor;
    using NamedColumn = revision_row_datatypes::NamedColumn;

    int32_t rev_page;
    int32_t rev_text_id;
    var_string<1024> rev_comment;
    int32_t rev_user;
    var_string<255> rev_user_text;
    var_string<14> rev_timestamp;
    int32_t rev_minor_edit;
    int32_t rev_deleted;
    int32_t rev_len;
    int32_t rev_parent_id;
};

enum class NamedColumn : int {
    rev_page = 0,
    rev_text_id,
    rev_comment,
    rev_user,
    rev_user_text,
    rev_timestamp,
    rev_minor_edit,
    rev_deleted,
    rev_len,
    rev_parent_id,
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
    if constexpr (Column < NamedColumn::rev_text_id) {
        return NamedColumn::rev_page;
    }
    if constexpr (Column < NamedColumn::rev_comment) {
        return NamedColumn::rev_text_id;
    }
    if constexpr (Column < NamedColumn::rev_user) {
        return NamedColumn::rev_comment;
    }
    if constexpr (Column < NamedColumn::rev_user_text) {
        return NamedColumn::rev_user;
    }
    if constexpr (Column < NamedColumn::rev_timestamp) {
        return NamedColumn::rev_user_text;
    }
    if constexpr (Column < NamedColumn::rev_minor_edit) {
        return NamedColumn::rev_timestamp;
    }
    if constexpr (Column < NamedColumn::rev_deleted) {
        return NamedColumn::rev_minor_edit;
    }
    if constexpr (Column < NamedColumn::rev_len) {
        return NamedColumn::rev_deleted;
    }
    if constexpr (Column < NamedColumn::rev_parent_id) {
        return NamedColumn::rev_len;
    }
    return NamedColumn::rev_parent_id;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::rev_page> {
    using NamedColumn = revision_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::rev_page;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rev_text_id> {
    using NamedColumn = revision_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::rev_text_id;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rev_comment> {
    using NamedColumn = revision_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<1024>;
    using value_type = var_string<1024>;
    static constexpr NamedColumn Column = NamedColumn::rev_comment;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rev_user> {
    using NamedColumn = revision_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::rev_user;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rev_user_text> {
    using NamedColumn = revision_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<255>;
    using value_type = var_string<255>;
    static constexpr NamedColumn Column = NamedColumn::rev_user_text;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rev_timestamp> {
    using NamedColumn = revision_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<14>;
    using value_type = var_string<14>;
    static constexpr NamedColumn Column = NamedColumn::rev_timestamp;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rev_minor_edit> {
    using NamedColumn = revision_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::rev_minor_edit;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rev_deleted> {
    using NamedColumn = revision_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::rev_deleted;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rev_len> {
    using NamedColumn = revision_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::rev_len;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::rev_parent_id> {
    using NamedColumn = revision_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::rev_parent_id;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            revision_row* ptr) {
        if constexpr (NamedColumn::rev_page <= Column && Column < NamedColumn::rev_text_id) {
            return ptr->rev_page;
        }
        if constexpr (NamedColumn::rev_text_id <= Column && Column < NamedColumn::rev_comment) {
            return ptr->rev_text_id;
        }
        if constexpr (NamedColumn::rev_comment <= Column && Column < NamedColumn::rev_user) {
            return ptr->rev_comment;
        }
        if constexpr (NamedColumn::rev_user <= Column && Column < NamedColumn::rev_user_text) {
            return ptr->rev_user;
        }
        if constexpr (NamedColumn::rev_user_text <= Column && Column < NamedColumn::rev_timestamp) {
            return ptr->rev_user_text;
        }
        if constexpr (NamedColumn::rev_timestamp <= Column && Column < NamedColumn::rev_minor_edit) {
            return ptr->rev_timestamp;
        }
        if constexpr (NamedColumn::rev_minor_edit <= Column && Column < NamedColumn::rev_deleted) {
            return ptr->rev_minor_edit;
        }
        if constexpr (NamedColumn::rev_deleted <= Column && Column < NamedColumn::rev_len) {
            return ptr->rev_deleted;
        }
        if constexpr (NamedColumn::rev_len <= Column && Column < NamedColumn::rev_parent_id) {
            return ptr->rev_len;
        }
        if constexpr (NamedColumn::rev_parent_id <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->rev_parent_id;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 10;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(revision_row* dest, revision_row* src) {
        if constexpr(Cell == 0) {
            dest->rev_page = src->rev_page;
            dest->rev_text_id = src->rev_text_id;
            dest->rev_comment = src->rev_comment;
            dest->rev_user = src->rev_user;
            dest->rev_user_text = src->rev_user_text;
            dest->rev_timestamp = src->rev_timestamp;
            dest->rev_minor_edit = src->rev_minor_edit;
            dest->rev_deleted = src->rev_deleted;
            dest->rev_len = src->rev_len;
            dest->rev_parent_id = src->rev_parent_id;
        }
    }
};

class RecordAccessor {
public:
    using NamedColumn = revision_row_datatypes::NamedColumn;
    //using SplitTable = revision_row_datatypes::SplitTable;
    using SplitType = revision_row_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<revision_row>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = revision_row;
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

    void copy_into(revision_row* vptr, int index=0) {
        copy_cell(index, 0, vptr, vptrs_[0]);
    }

    inline typename accessor_info<NamedColumn::rev_page>::value_type& rev_page() {
        return vptrs_[cell_of(NamedColumn::rev_page)]->rev_page;
    }

    inline const typename accessor_info<NamedColumn::rev_page>::value_type& rev_page() const {
        return vptrs_[cell_of(NamedColumn::rev_page)]->rev_page;
    }

    inline typename accessor_info<NamedColumn::rev_text_id>::value_type& rev_text_id() {
        return vptrs_[cell_of(NamedColumn::rev_text_id)]->rev_text_id;
    }

    inline const typename accessor_info<NamedColumn::rev_text_id>::value_type& rev_text_id() const {
        return vptrs_[cell_of(NamedColumn::rev_text_id)]->rev_text_id;
    }

    inline typename accessor_info<NamedColumn::rev_comment>::value_type& rev_comment() {
        return vptrs_[cell_of(NamedColumn::rev_comment)]->rev_comment;
    }

    inline const typename accessor_info<NamedColumn::rev_comment>::value_type& rev_comment() const {
        return vptrs_[cell_of(NamedColumn::rev_comment)]->rev_comment;
    }

    inline typename accessor_info<NamedColumn::rev_user>::value_type& rev_user() {
        return vptrs_[cell_of(NamedColumn::rev_user)]->rev_user;
    }

    inline const typename accessor_info<NamedColumn::rev_user>::value_type& rev_user() const {
        return vptrs_[cell_of(NamedColumn::rev_user)]->rev_user;
    }

    inline typename accessor_info<NamedColumn::rev_user_text>::value_type& rev_user_text() {
        return vptrs_[cell_of(NamedColumn::rev_user_text)]->rev_user_text;
    }

    inline const typename accessor_info<NamedColumn::rev_user_text>::value_type& rev_user_text() const {
        return vptrs_[cell_of(NamedColumn::rev_user_text)]->rev_user_text;
    }

    inline typename accessor_info<NamedColumn::rev_timestamp>::value_type& rev_timestamp() {
        return vptrs_[cell_of(NamedColumn::rev_timestamp)]->rev_timestamp;
    }

    inline const typename accessor_info<NamedColumn::rev_timestamp>::value_type& rev_timestamp() const {
        return vptrs_[cell_of(NamedColumn::rev_timestamp)]->rev_timestamp;
    }

    inline typename accessor_info<NamedColumn::rev_minor_edit>::value_type& rev_minor_edit() {
        return vptrs_[cell_of(NamedColumn::rev_minor_edit)]->rev_minor_edit;
    }

    inline const typename accessor_info<NamedColumn::rev_minor_edit>::value_type& rev_minor_edit() const {
        return vptrs_[cell_of(NamedColumn::rev_minor_edit)]->rev_minor_edit;
    }

    inline typename accessor_info<NamedColumn::rev_deleted>::value_type& rev_deleted() {
        return vptrs_[cell_of(NamedColumn::rev_deleted)]->rev_deleted;
    }

    inline const typename accessor_info<NamedColumn::rev_deleted>::value_type& rev_deleted() const {
        return vptrs_[cell_of(NamedColumn::rev_deleted)]->rev_deleted;
    }

    inline typename accessor_info<NamedColumn::rev_len>::value_type& rev_len() {
        return vptrs_[cell_of(NamedColumn::rev_len)]->rev_len;
    }

    inline const typename accessor_info<NamedColumn::rev_len>::value_type& rev_len() const {
        return vptrs_[cell_of(NamedColumn::rev_len)]->rev_len;
    }

    inline typename accessor_info<NamedColumn::rev_parent_id>::value_type& rev_parent_id() {
        return vptrs_[cell_of(NamedColumn::rev_parent_id)]->rev_parent_id;
    }

    inline const typename accessor_info<NamedColumn::rev_parent_id>::value_type& rev_parent_id() const {
        return vptrs_[cell_of(NamedColumn::rev_parent_id)]->rev_parent_id;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::rev_page <= Column && Column < NamedColumn::rev_text_id) {
            return vptrs_[cell_of(NamedColumn::rev_page)]->rev_page;
        }
        if constexpr (NamedColumn::rev_text_id <= Column && Column < NamedColumn::rev_comment) {
            return vptrs_[cell_of(NamedColumn::rev_text_id)]->rev_text_id;
        }
        if constexpr (NamedColumn::rev_comment <= Column && Column < NamedColumn::rev_user) {
            return vptrs_[cell_of(NamedColumn::rev_comment)]->rev_comment;
        }
        if constexpr (NamedColumn::rev_user <= Column && Column < NamedColumn::rev_user_text) {
            return vptrs_[cell_of(NamedColumn::rev_user)]->rev_user;
        }
        if constexpr (NamedColumn::rev_user_text <= Column && Column < NamedColumn::rev_timestamp) {
            return vptrs_[cell_of(NamedColumn::rev_user_text)]->rev_user_text;
        }
        if constexpr (NamedColumn::rev_timestamp <= Column && Column < NamedColumn::rev_minor_edit) {
            return vptrs_[cell_of(NamedColumn::rev_timestamp)]->rev_timestamp;
        }
        if constexpr (NamedColumn::rev_minor_edit <= Column && Column < NamedColumn::rev_deleted) {
            return vptrs_[cell_of(NamedColumn::rev_minor_edit)]->rev_minor_edit;
        }
        if constexpr (NamedColumn::rev_deleted <= Column && Column < NamedColumn::rev_len) {
            return vptrs_[cell_of(NamedColumn::rev_deleted)]->rev_deleted;
        }
        if constexpr (NamedColumn::rev_len <= Column && Column < NamedColumn::rev_parent_id) {
            return vptrs_[cell_of(NamedColumn::rev_len)]->rev_len;
        }
        if constexpr (NamedColumn::rev_parent_id <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::rev_parent_id)]->rev_parent_id;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<revision_row*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace revision_row_datatypes

using revision_row = revision_row_datatypes::revision_row;

namespace text_row_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct text_row {
    using RecordAccessor = text_row_datatypes::RecordAccessor;
    using NamedColumn = text_row_datatypes::NamedColumn;

    char * old_text;
    var_string<30> old_flags;
    int32_t old_page;
};

enum class NamedColumn : int {
    old_text = 0,
    old_flags,
    old_page,
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
    if constexpr (Column < NamedColumn::old_flags) {
        return NamedColumn::old_text;
    }
    if constexpr (Column < NamedColumn::old_page) {
        return NamedColumn::old_flags;
    }
    return NamedColumn::old_page;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::old_text> {
    using NamedColumn = text_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = char *;
    using value_type = char *;
    static constexpr NamedColumn Column = NamedColumn::old_text;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::old_flags> {
    using NamedColumn = text_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<30>;
    using value_type = var_string<30>;
    static constexpr NamedColumn Column = NamedColumn::old_flags;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::old_page> {
    using NamedColumn = text_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::old_page;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            text_row* ptr) {
        if constexpr (NamedColumn::old_text <= Column && Column < NamedColumn::old_flags) {
            return ptr->old_text;
        }
        if constexpr (NamedColumn::old_flags <= Column && Column < NamedColumn::old_page) {
            return ptr->old_flags;
        }
        if constexpr (NamedColumn::old_page <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->old_page;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 3;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(text_row* dest, text_row* src) {
        if constexpr(Cell == 0) {
            dest->old_text = src->old_text;
            dest->old_flags = src->old_flags;
            dest->old_page = src->old_page;
        }
    }
};

class RecordAccessor {
public:
    using NamedColumn = text_row_datatypes::NamedColumn;
    //using SplitTable = text_row_datatypes::SplitTable;
    using SplitType = text_row_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<text_row>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = text_row;
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

    void copy_into(text_row* vptr, int index=0) {
        copy_cell(index, 0, vptr, vptrs_[0]);
    }

    inline typename accessor_info<NamedColumn::old_text>::value_type& old_text() {
        return vptrs_[cell_of(NamedColumn::old_text)]->old_text;
    }

    inline const typename accessor_info<NamedColumn::old_text>::value_type& old_text() const {
        return vptrs_[cell_of(NamedColumn::old_text)]->old_text;
    }

    inline typename accessor_info<NamedColumn::old_flags>::value_type& old_flags() {
        return vptrs_[cell_of(NamedColumn::old_flags)]->old_flags;
    }

    inline const typename accessor_info<NamedColumn::old_flags>::value_type& old_flags() const {
        return vptrs_[cell_of(NamedColumn::old_flags)]->old_flags;
    }

    inline typename accessor_info<NamedColumn::old_page>::value_type& old_page() {
        return vptrs_[cell_of(NamedColumn::old_page)]->old_page;
    }

    inline const typename accessor_info<NamedColumn::old_page>::value_type& old_page() const {
        return vptrs_[cell_of(NamedColumn::old_page)]->old_page;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::old_text <= Column && Column < NamedColumn::old_flags) {
            return vptrs_[cell_of(NamedColumn::old_text)]->old_text;
        }
        if constexpr (NamedColumn::old_flags <= Column && Column < NamedColumn::old_page) {
            return vptrs_[cell_of(NamedColumn::old_flags)]->old_flags;
        }
        if constexpr (NamedColumn::old_page <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::old_page)]->old_page;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<text_row*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace text_row_datatypes

using text_row = text_row_datatypes::text_row;

namespace useracct_row_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct useracct_row {
    using RecordAccessor = useracct_row_datatypes::RecordAccessor;
    using NamedColumn = useracct_row_datatypes::NamedColumn;

    var_string<255> user_name;
    var_string<255> user_real_name;
    var_string<32> user_password;
    var_string<32> user_newpassword;
    var_string<14> user_newpass_time;
    var_string<40> user_email;
    var_string<255> user_options;
    var_string<32> user_token;
    var_string<32> user_email_authenticated;
    var_string<32> user_email_token;
    var_string<14> user_email_token_expires;
    var_string<14> user_registration;
    var_string<14> user_touched;
    int32_t user_editcount;
};

enum class NamedColumn : int {
    user_name = 0,
    user_real_name,
    user_password,
    user_newpassword,
    user_newpass_time,
    user_email,
    user_options,
    user_token,
    user_email_authenticated,
    user_email_token,
    user_email_token_expires,
    user_registration,
    user_touched,
    user_editcount,
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
    if constexpr (Column < NamedColumn::user_real_name) {
        return NamedColumn::user_name;
    }
    if constexpr (Column < NamedColumn::user_password) {
        return NamedColumn::user_real_name;
    }
    if constexpr (Column < NamedColumn::user_newpassword) {
        return NamedColumn::user_password;
    }
    if constexpr (Column < NamedColumn::user_newpass_time) {
        return NamedColumn::user_newpassword;
    }
    if constexpr (Column < NamedColumn::user_email) {
        return NamedColumn::user_newpass_time;
    }
    if constexpr (Column < NamedColumn::user_options) {
        return NamedColumn::user_email;
    }
    if constexpr (Column < NamedColumn::user_token) {
        return NamedColumn::user_options;
    }
    if constexpr (Column < NamedColumn::user_email_authenticated) {
        return NamedColumn::user_token;
    }
    if constexpr (Column < NamedColumn::user_email_token) {
        return NamedColumn::user_email_authenticated;
    }
    if constexpr (Column < NamedColumn::user_email_token_expires) {
        return NamedColumn::user_email_token;
    }
    if constexpr (Column < NamedColumn::user_registration) {
        return NamedColumn::user_email_token_expires;
    }
    if constexpr (Column < NamedColumn::user_touched) {
        return NamedColumn::user_registration;
    }
    if constexpr (Column < NamedColumn::user_editcount) {
        return NamedColumn::user_touched;
    }
    return NamedColumn::user_editcount;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::user_name> {
    using NamedColumn = useracct_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<255>;
    using value_type = var_string<255>;
    static constexpr NamedColumn Column = NamedColumn::user_name;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::user_real_name> {
    using NamedColumn = useracct_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<255>;
    using value_type = var_string<255>;
    static constexpr NamedColumn Column = NamedColumn::user_real_name;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::user_password> {
    using NamedColumn = useracct_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<32>;
    using value_type = var_string<32>;
    static constexpr NamedColumn Column = NamedColumn::user_password;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::user_newpassword> {
    using NamedColumn = useracct_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<32>;
    using value_type = var_string<32>;
    static constexpr NamedColumn Column = NamedColumn::user_newpassword;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::user_newpass_time> {
    using NamedColumn = useracct_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<14>;
    using value_type = var_string<14>;
    static constexpr NamedColumn Column = NamedColumn::user_newpass_time;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::user_email> {
    using NamedColumn = useracct_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<40>;
    using value_type = var_string<40>;
    static constexpr NamedColumn Column = NamedColumn::user_email;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::user_options> {
    using NamedColumn = useracct_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<255>;
    using value_type = var_string<255>;
    static constexpr NamedColumn Column = NamedColumn::user_options;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::user_token> {
    using NamedColumn = useracct_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<32>;
    using value_type = var_string<32>;
    static constexpr NamedColumn Column = NamedColumn::user_token;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::user_email_authenticated> {
    using NamedColumn = useracct_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<32>;
    using value_type = var_string<32>;
    static constexpr NamedColumn Column = NamedColumn::user_email_authenticated;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::user_email_token> {
    using NamedColumn = useracct_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<32>;
    using value_type = var_string<32>;
    static constexpr NamedColumn Column = NamedColumn::user_email_token;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::user_email_token_expires> {
    using NamedColumn = useracct_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<14>;
    using value_type = var_string<14>;
    static constexpr NamedColumn Column = NamedColumn::user_email_token_expires;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::user_registration> {
    using NamedColumn = useracct_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<14>;
    using value_type = var_string<14>;
    static constexpr NamedColumn Column = NamedColumn::user_registration;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::user_touched> {
    using NamedColumn = useracct_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<14>;
    using value_type = var_string<14>;
    static constexpr NamedColumn Column = NamedColumn::user_touched;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::user_editcount> {
    using NamedColumn = useracct_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::user_editcount;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            useracct_row* ptr) {
        if constexpr (NamedColumn::user_name <= Column && Column < NamedColumn::user_real_name) {
            return ptr->user_name;
        }
        if constexpr (NamedColumn::user_real_name <= Column && Column < NamedColumn::user_password) {
            return ptr->user_real_name;
        }
        if constexpr (NamedColumn::user_password <= Column && Column < NamedColumn::user_newpassword) {
            return ptr->user_password;
        }
        if constexpr (NamedColumn::user_newpassword <= Column && Column < NamedColumn::user_newpass_time) {
            return ptr->user_newpassword;
        }
        if constexpr (NamedColumn::user_newpass_time <= Column && Column < NamedColumn::user_email) {
            return ptr->user_newpass_time;
        }
        if constexpr (NamedColumn::user_email <= Column && Column < NamedColumn::user_options) {
            return ptr->user_email;
        }
        if constexpr (NamedColumn::user_options <= Column && Column < NamedColumn::user_token) {
            return ptr->user_options;
        }
        if constexpr (NamedColumn::user_token <= Column && Column < NamedColumn::user_email_authenticated) {
            return ptr->user_token;
        }
        if constexpr (NamedColumn::user_email_authenticated <= Column && Column < NamedColumn::user_email_token) {
            return ptr->user_email_authenticated;
        }
        if constexpr (NamedColumn::user_email_token <= Column && Column < NamedColumn::user_email_token_expires) {
            return ptr->user_email_token;
        }
        if constexpr (NamedColumn::user_email_token_expires <= Column && Column < NamedColumn::user_registration) {
            return ptr->user_email_token_expires;
        }
        if constexpr (NamedColumn::user_registration <= Column && Column < NamedColumn::user_touched) {
            return ptr->user_registration;
        }
        if constexpr (NamedColumn::user_touched <= Column && Column < NamedColumn::user_editcount) {
            return ptr->user_touched;
        }
        if constexpr (NamedColumn::user_editcount <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->user_editcount;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 14;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(useracct_row* dest, useracct_row* src) {
        if constexpr(Cell == 0) {
            dest->user_name = src->user_name;
            dest->user_real_name = src->user_real_name;
            dest->user_password = src->user_password;
            dest->user_newpassword = src->user_newpassword;
            dest->user_newpass_time = src->user_newpass_time;
            dest->user_email = src->user_email;
            dest->user_options = src->user_options;
            dest->user_token = src->user_token;
            dest->user_email_authenticated = src->user_email_authenticated;
            dest->user_email_token = src->user_email_token;
            dest->user_email_token_expires = src->user_email_token_expires;
            dest->user_registration = src->user_registration;
            dest->user_touched = src->user_touched;
            dest->user_editcount = src->user_editcount;
        }
    }
};

template <>
struct SplitPolicy<1> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 1) {
            return 12;
        }
        if (cell == 0) {
            return 2;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(useracct_row* dest, useracct_row* src) {
        if constexpr(Cell == 1) {
            dest->user_name = src->user_name;
            dest->user_real_name = src->user_real_name;
            dest->user_password = src->user_password;
            dest->user_newpassword = src->user_newpassword;
            dest->user_newpass_time = src->user_newpass_time;
            dest->user_email = src->user_email;
            dest->user_options = src->user_options;
            dest->user_token = src->user_token;
            dest->user_email_authenticated = src->user_email_authenticated;
            dest->user_email_token = src->user_email_token;
            dest->user_email_token_expires = src->user_email_token_expires;
            dest->user_registration = src->user_registration;
        }
        if constexpr(Cell == 0) {
            dest->user_touched = src->user_touched;
            dest->user_editcount = src->user_editcount;
        }
    }
};

class RecordAccessor {
public:
    using NamedColumn = useracct_row_datatypes::NamedColumn;
    //using SplitTable = useracct_row_datatypes::SplitTable;
    using SplitType = useracct_row_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<useracct_row>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = useracct_row;
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

    void copy_into(useracct_row* vptr, int index=0) {
        copy_cell(index, 0, vptr, vptrs_[0]);
        copy_cell(index, 1, vptr, vptrs_[1]);
    }

    inline typename accessor_info<NamedColumn::user_name>::value_type& user_name() {
        return vptrs_[cell_of(NamedColumn::user_name)]->user_name;
    }

    inline const typename accessor_info<NamedColumn::user_name>::value_type& user_name() const {
        return vptrs_[cell_of(NamedColumn::user_name)]->user_name;
    }

    inline typename accessor_info<NamedColumn::user_real_name>::value_type& user_real_name() {
        return vptrs_[cell_of(NamedColumn::user_real_name)]->user_real_name;
    }

    inline const typename accessor_info<NamedColumn::user_real_name>::value_type& user_real_name() const {
        return vptrs_[cell_of(NamedColumn::user_real_name)]->user_real_name;
    }

    inline typename accessor_info<NamedColumn::user_password>::value_type& user_password() {
        return vptrs_[cell_of(NamedColumn::user_password)]->user_password;
    }

    inline const typename accessor_info<NamedColumn::user_password>::value_type& user_password() const {
        return vptrs_[cell_of(NamedColumn::user_password)]->user_password;
    }

    inline typename accessor_info<NamedColumn::user_newpassword>::value_type& user_newpassword() {
        return vptrs_[cell_of(NamedColumn::user_newpassword)]->user_newpassword;
    }

    inline const typename accessor_info<NamedColumn::user_newpassword>::value_type& user_newpassword() const {
        return vptrs_[cell_of(NamedColumn::user_newpassword)]->user_newpassword;
    }

    inline typename accessor_info<NamedColumn::user_newpass_time>::value_type& user_newpass_time() {
        return vptrs_[cell_of(NamedColumn::user_newpass_time)]->user_newpass_time;
    }

    inline const typename accessor_info<NamedColumn::user_newpass_time>::value_type& user_newpass_time() const {
        return vptrs_[cell_of(NamedColumn::user_newpass_time)]->user_newpass_time;
    }

    inline typename accessor_info<NamedColumn::user_email>::value_type& user_email() {
        return vptrs_[cell_of(NamedColumn::user_email)]->user_email;
    }

    inline const typename accessor_info<NamedColumn::user_email>::value_type& user_email() const {
        return vptrs_[cell_of(NamedColumn::user_email)]->user_email;
    }

    inline typename accessor_info<NamedColumn::user_options>::value_type& user_options() {
        return vptrs_[cell_of(NamedColumn::user_options)]->user_options;
    }

    inline const typename accessor_info<NamedColumn::user_options>::value_type& user_options() const {
        return vptrs_[cell_of(NamedColumn::user_options)]->user_options;
    }

    inline typename accessor_info<NamedColumn::user_token>::value_type& user_token() {
        return vptrs_[cell_of(NamedColumn::user_token)]->user_token;
    }

    inline const typename accessor_info<NamedColumn::user_token>::value_type& user_token() const {
        return vptrs_[cell_of(NamedColumn::user_token)]->user_token;
    }

    inline typename accessor_info<NamedColumn::user_email_authenticated>::value_type& user_email_authenticated() {
        return vptrs_[cell_of(NamedColumn::user_email_authenticated)]->user_email_authenticated;
    }

    inline const typename accessor_info<NamedColumn::user_email_authenticated>::value_type& user_email_authenticated() const {
        return vptrs_[cell_of(NamedColumn::user_email_authenticated)]->user_email_authenticated;
    }

    inline typename accessor_info<NamedColumn::user_email_token>::value_type& user_email_token() {
        return vptrs_[cell_of(NamedColumn::user_email_token)]->user_email_token;
    }

    inline const typename accessor_info<NamedColumn::user_email_token>::value_type& user_email_token() const {
        return vptrs_[cell_of(NamedColumn::user_email_token)]->user_email_token;
    }

    inline typename accessor_info<NamedColumn::user_email_token_expires>::value_type& user_email_token_expires() {
        return vptrs_[cell_of(NamedColumn::user_email_token_expires)]->user_email_token_expires;
    }

    inline const typename accessor_info<NamedColumn::user_email_token_expires>::value_type& user_email_token_expires() const {
        return vptrs_[cell_of(NamedColumn::user_email_token_expires)]->user_email_token_expires;
    }

    inline typename accessor_info<NamedColumn::user_registration>::value_type& user_registration() {
        return vptrs_[cell_of(NamedColumn::user_registration)]->user_registration;
    }

    inline const typename accessor_info<NamedColumn::user_registration>::value_type& user_registration() const {
        return vptrs_[cell_of(NamedColumn::user_registration)]->user_registration;
    }

    inline typename accessor_info<NamedColumn::user_touched>::value_type& user_touched() {
        return vptrs_[cell_of(NamedColumn::user_touched)]->user_touched;
    }

    inline const typename accessor_info<NamedColumn::user_touched>::value_type& user_touched() const {
        return vptrs_[cell_of(NamedColumn::user_touched)]->user_touched;
    }

    inline typename accessor_info<NamedColumn::user_editcount>::value_type& user_editcount() {
        return vptrs_[cell_of(NamedColumn::user_editcount)]->user_editcount;
    }

    inline const typename accessor_info<NamedColumn::user_editcount>::value_type& user_editcount() const {
        return vptrs_[cell_of(NamedColumn::user_editcount)]->user_editcount;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::user_name <= Column && Column < NamedColumn::user_real_name) {
            return vptrs_[cell_of(NamedColumn::user_name)]->user_name;
        }
        if constexpr (NamedColumn::user_real_name <= Column && Column < NamedColumn::user_password) {
            return vptrs_[cell_of(NamedColumn::user_real_name)]->user_real_name;
        }
        if constexpr (NamedColumn::user_password <= Column && Column < NamedColumn::user_newpassword) {
            return vptrs_[cell_of(NamedColumn::user_password)]->user_password;
        }
        if constexpr (NamedColumn::user_newpassword <= Column && Column < NamedColumn::user_newpass_time) {
            return vptrs_[cell_of(NamedColumn::user_newpassword)]->user_newpassword;
        }
        if constexpr (NamedColumn::user_newpass_time <= Column && Column < NamedColumn::user_email) {
            return vptrs_[cell_of(NamedColumn::user_newpass_time)]->user_newpass_time;
        }
        if constexpr (NamedColumn::user_email <= Column && Column < NamedColumn::user_options) {
            return vptrs_[cell_of(NamedColumn::user_email)]->user_email;
        }
        if constexpr (NamedColumn::user_options <= Column && Column < NamedColumn::user_token) {
            return vptrs_[cell_of(NamedColumn::user_options)]->user_options;
        }
        if constexpr (NamedColumn::user_token <= Column && Column < NamedColumn::user_email_authenticated) {
            return vptrs_[cell_of(NamedColumn::user_token)]->user_token;
        }
        if constexpr (NamedColumn::user_email_authenticated <= Column && Column < NamedColumn::user_email_token) {
            return vptrs_[cell_of(NamedColumn::user_email_authenticated)]->user_email_authenticated;
        }
        if constexpr (NamedColumn::user_email_token <= Column && Column < NamedColumn::user_email_token_expires) {
            return vptrs_[cell_of(NamedColumn::user_email_token)]->user_email_token;
        }
        if constexpr (NamedColumn::user_email_token_expires <= Column && Column < NamedColumn::user_registration) {
            return vptrs_[cell_of(NamedColumn::user_email_token_expires)]->user_email_token_expires;
        }
        if constexpr (NamedColumn::user_registration <= Column && Column < NamedColumn::user_touched) {
            return vptrs_[cell_of(NamedColumn::user_registration)]->user_registration;
        }
        if constexpr (NamedColumn::user_touched <= Column && Column < NamedColumn::user_editcount) {
            return vptrs_[cell_of(NamedColumn::user_touched)]->user_touched;
        }
        if constexpr (NamedColumn::user_editcount <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::user_editcount)]->user_editcount;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<useracct_row*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace useracct_row_datatypes

using useracct_row = useracct_row_datatypes::useracct_row;

namespace useracct_idx_row_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct useracct_idx_row {
    using RecordAccessor = useracct_idx_row_datatypes::RecordAccessor;
    using NamedColumn = useracct_idx_row_datatypes::NamedColumn;

    int32_t user_id;
};

enum class NamedColumn : int {
    user_id = 0,
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
    return NamedColumn::user_id;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::user_id> {
    using NamedColumn = useracct_idx_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::user_id;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            useracct_idx_row* ptr) {
        if constexpr (NamedColumn::user_id <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->user_id;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 1;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(useracct_idx_row* dest, useracct_idx_row* src) {
        if constexpr(Cell == 0) {
            dest->user_id = src->user_id;
        }
    }
};

class RecordAccessor {
public:
    using NamedColumn = useracct_idx_row_datatypes::NamedColumn;
    //using SplitTable = useracct_idx_row_datatypes::SplitTable;
    using SplitType = useracct_idx_row_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<useracct_idx_row>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = useracct_idx_row;
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

    void copy_into(useracct_idx_row* vptr, int index=0) {
        copy_cell(index, 0, vptr, vptrs_[0]);
    }

    inline typename accessor_info<NamedColumn::user_id>::value_type& user_id() {
        return vptrs_[cell_of(NamedColumn::user_id)]->user_id;
    }

    inline const typename accessor_info<NamedColumn::user_id>::value_type& user_id() const {
        return vptrs_[cell_of(NamedColumn::user_id)]->user_id;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::user_id <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::user_id)]->user_id;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<useracct_idx_row*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace useracct_idx_row_datatypes

using useracct_idx_row = useracct_idx_row_datatypes::useracct_idx_row;

namespace user_groups_row_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct user_groups_row {
    using RecordAccessor = user_groups_row_datatypes::RecordAccessor;
    using NamedColumn = user_groups_row_datatypes::NamedColumn;

    var_string<16> ug_group;
};

enum class NamedColumn : int {
    ug_group = 0,
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
    return NamedColumn::ug_group;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::ug_group> {
    using NamedColumn = user_groups_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<16>;
    using value_type = var_string<16>;
    static constexpr NamedColumn Column = NamedColumn::ug_group;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            user_groups_row* ptr) {
        if constexpr (NamedColumn::ug_group <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->ug_group;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 1;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(user_groups_row* dest, user_groups_row* src) {
        if constexpr(Cell == 0) {
            dest->ug_group = src->ug_group;
        }
    }
};

class RecordAccessor {
public:
    using NamedColumn = user_groups_row_datatypes::NamedColumn;
    //using SplitTable = user_groups_row_datatypes::SplitTable;
    using SplitType = user_groups_row_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<user_groups_row>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = user_groups_row;
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

    void copy_into(user_groups_row* vptr, int index=0) {
        copy_cell(index, 0, vptr, vptrs_[0]);
    }

    inline typename accessor_info<NamedColumn::ug_group>::value_type& ug_group() {
        return vptrs_[cell_of(NamedColumn::ug_group)]->ug_group;
    }

    inline const typename accessor_info<NamedColumn::ug_group>::value_type& ug_group() const {
        return vptrs_[cell_of(NamedColumn::ug_group)]->ug_group;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::ug_group <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::ug_group)]->ug_group;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<user_groups_row*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace user_groups_row_datatypes

using user_groups_row = user_groups_row_datatypes::user_groups_row;

namespace watchlist_row_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct watchlist_row {
    using RecordAccessor = watchlist_row_datatypes::RecordAccessor;
    using NamedColumn = watchlist_row_datatypes::NamedColumn;

    var_string<14> wl_notificationtimestamp;
};

enum class NamedColumn : int {
    wl_notificationtimestamp = 0,
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
    return NamedColumn::wl_notificationtimestamp;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::wl_notificationtimestamp> {
    using NamedColumn = watchlist_row_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = var_string<14>;
    using value_type = var_string<14>;
    static constexpr NamedColumn Column = NamedColumn::wl_notificationtimestamp;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            watchlist_row* ptr) {
        if constexpr (NamedColumn::wl_notificationtimestamp <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->wl_notificationtimestamp;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }
};

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 0) {
            return 1;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(watchlist_row* dest, watchlist_row* src) {
        if constexpr(Cell == 0) {
            dest->wl_notificationtimestamp = src->wl_notificationtimestamp;
        }
    }
};

class RecordAccessor {
public:
    using NamedColumn = watchlist_row_datatypes::NamedColumn;
    //using SplitTable = watchlist_row_datatypes::SplitTable;
    using SplitType = watchlist_row_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<watchlist_row>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = watchlist_row;
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

    void copy_into(watchlist_row* vptr, int index=0) {
        copy_cell(index, 0, vptr, vptrs_[0]);
    }

    inline typename accessor_info<NamedColumn::wl_notificationtimestamp>::value_type& wl_notificationtimestamp() {
        return vptrs_[cell_of(NamedColumn::wl_notificationtimestamp)]->wl_notificationtimestamp;
    }

    inline const typename accessor_info<NamedColumn::wl_notificationtimestamp>::value_type& wl_notificationtimestamp() const {
        return vptrs_[cell_of(NamedColumn::wl_notificationtimestamp)]->wl_notificationtimestamp;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::wl_notificationtimestamp <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::wl_notificationtimestamp)]->wl_notificationtimestamp;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<watchlist_row*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace watchlist_row_datatypes

using watchlist_row = watchlist_row_datatypes::watchlist_row;

