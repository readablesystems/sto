#pragma once

#include <type_traits>
//#include "AdapterStructs.hh"

namespace page_value_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct page_value {
    using RecordAccessor = page_value_datatypes::RecordAccessor;
    using NamedColumn = page_value_datatypes::NamedColumn;

    int32_t page_id;
    int32_t likes;
};

enum class NamedColumn : int {
    page_id = 0,
    likes,
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
    if constexpr (Column < NamedColumn::likes) {
        return NamedColumn::page_id;
    }
    return NamedColumn::likes;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::page_id> {
    using NamedColumn = page_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::page_id;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::likes> {
    using NamedColumn = page_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::likes;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            page_value* ptr) {
        if constexpr (NamedColumn::page_id <= Column && Column < NamedColumn::likes) {
            return ptr->page_id;
        }
        if constexpr (NamedColumn::likes <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->likes;
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
    inline static constexpr void copy_cell(page_value* dest, page_value* src) {
        if constexpr(Cell == 0) {
            dest->page_id = src->page_id;
            dest->likes = src->likes;
        }
    }
};

template <>
struct SplitPolicy<1> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 1, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 1) {
            return 1;
        }
        if (cell == 0) {
            return 1;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(page_value* dest, page_value* src) {
        if constexpr(Cell == 1) {
            dest->page_id = src->page_id;
        }
        if constexpr(Cell == 0) {
            dest->likes = src->likes;
        }
    }
};

class RecordAccessor {
public:
    using NamedColumn = page_value_datatypes::NamedColumn;
    //using SplitTable = page_value_datatypes::SplitTable;
    using SplitType = page_value_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<page_value>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = page_value;
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

    void copy_into(page_value* vptr, int index=0) {
        copy_cell(index, 0, vptr, vptrs_[0]);
        copy_cell(index, 1, vptr, vptrs_[1]);
    }

    inline typename accessor_info<NamedColumn::page_id>::value_type& page_id() {
        return vptrs_[cell_of(NamedColumn::page_id)]->page_id;
    }

    inline const typename accessor_info<NamedColumn::page_id>::value_type& page_id() const {
        return vptrs_[cell_of(NamedColumn::page_id)]->page_id;
    }

    inline typename accessor_info<NamedColumn::likes>::value_type& likes() {
        return vptrs_[cell_of(NamedColumn::likes)]->likes;
    }

    inline const typename accessor_info<NamedColumn::likes>::value_type& likes() const {
        return vptrs_[cell_of(NamedColumn::likes)]->likes;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::page_id <= Column && Column < NamedColumn::likes) {
            return vptrs_[cell_of(NamedColumn::page_id)]->page_id;
        }
        if constexpr (NamedColumn::likes <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::likes)]->likes;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<page_value*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace page_value_datatypes

using page_value = page_value_datatypes::page_value;

namespace like_value_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct like_value {
    using RecordAccessor = like_value_datatypes::RecordAccessor;
    using NamedColumn = like_value_datatypes::NamedColumn;

    int32_t user_id;
    int32_t page_id;
};

enum class NamedColumn : int {
    user_id = 0,
    page_id,
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
    if constexpr (Column < NamedColumn::page_id) {
        return NamedColumn::user_id;
    }
    return NamedColumn::page_id;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::user_id> {
    using NamedColumn = like_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int32_t;
    using value_type = int32_t;
    static constexpr NamedColumn Column = NamedColumn::user_id;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::page_id> {
    using NamedColumn = like_value_datatypes::NamedColumn;
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
            like_value* ptr) {
        if constexpr (NamedColumn::user_id <= Column && Column < NamedColumn::page_id) {
            return ptr->user_id;
        }
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
    inline static constexpr void copy_cell(like_value* dest, like_value* src) {
        if constexpr(Cell == 0) {
            dest->user_id = src->user_id;
            dest->page_id = src->page_id;
        }
    }
};

class RecordAccessor {
public:
    using NamedColumn = like_value_datatypes::NamedColumn;
    //using SplitTable = like_value_datatypes::SplitTable;
    using SplitType = like_value_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<like_value>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = like_value;
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

    void copy_into(like_value* vptr, int index=0) {
        copy_cell(index, 0, vptr, vptrs_[0]);
    }

    inline typename accessor_info<NamedColumn::user_id>::value_type& user_id() {
        return vptrs_[cell_of(NamedColumn::user_id)]->user_id;
    }

    inline const typename accessor_info<NamedColumn::user_id>::value_type& user_id() const {
        return vptrs_[cell_of(NamedColumn::user_id)]->user_id;
    }

    inline typename accessor_info<NamedColumn::page_id>::value_type& page_id() {
        return vptrs_[cell_of(NamedColumn::page_id)]->page_id;
    }

    inline const typename accessor_info<NamedColumn::page_id>::value_type& page_id() const {
        return vptrs_[cell_of(NamedColumn::page_id)]->page_id;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::user_id <= Column && Column < NamedColumn::page_id) {
            return vptrs_[cell_of(NamedColumn::user_id)]->user_id;
        }
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

    std::array<like_value*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace like_value_datatypes

using like_value = like_value_datatypes::like_value;

namespace page_value_split_structs {

struct page_value_cell0 {
    using NamedColumn = typename page_value_datatypes::page_value::NamedColumn;

    int32_t likes;
};

struct page_value_cell1 {
    using NamedColumn = typename page_value_datatypes::page_value::NamedColumn;

    int32_t page_id;
};

};  // namespace page_value_split_structs

namespace bench {
template <>
struct SplitParams<page_value_datatypes::page_value, false> {
    using split_type_list = std::tuple<page_value_datatypes::page_value>;
    using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
    static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

    static constexpr auto split_builder = std::make_tuple(
        [](const page_value_datatypes::page_value& in) -> page_value_datatypes::page_value {
            page_value_datatypes::page_value out;
            out.page_id = in.page_id;
            out.likes = in.likes;
            return out;
        }
    );

    static constexpr auto split_merger = std::make_tuple(
        [](page_value_datatypes::page_value* out, const page_value_datatypes::page_value& in) -> void {
            out->page_id = in.page_id;
            out->likes = in.likes;
        }
    );

    static constexpr auto map = [](int) -> int {
        return 0;
    };
};

template <typename Accessor>
class RecordAccessor<Accessor, page_value_datatypes::page_value, false> {
public:
    const int32_t& page_id() const {
        return impl().page_id_impl();
    }

    const int32_t& likes() const {
        return impl().likes_impl();
    }

    void copy_into(page_value_datatypes::page_value* dst) const {
        return impl().copy_into_impl(dst);
    }

private:
    const Accessor& impl() const {
        return *static_cast<const Accessor*>(this);
    }
};

template <>
class UniRecordAccessor<page_value_datatypes::page_value, false> : public RecordAccessor<UniRecordAccessor<page_value_datatypes::page_value, false>, page_value_datatypes::page_value, false> {
public:
    UniRecordAccessor() = default;
    UniRecordAccessor(const page_value_datatypes::page_value* const vptr) : vptr_(vptr) {}

    operator bool() const {
        return vptr_ != nullptr;
    }

private:
    const int32_t& page_id_impl() const {
        return vptr_->page_id;
    }

    const int32_t& likes_impl() const {
        return vptr_->likes;
    }

    void copy_into_impl(page_value_datatypes::page_value* dst) const {
        if (vptr_) {
            dst->page_id = vptr_->page_id;
            dst->likes = vptr_->likes;
        }
    }

    /*
    void copy_into_impl(page_value_datatypes::page_value* dst) const {
        if (vptr_) {
            memcpy(dst, vptr_, sizeof *dst);
        }
    }
    */

    const page_value_datatypes::page_value* vptr_;
    friend RecordAccessor<UniRecordAccessor<page_value_datatypes::page_value, false>, page_value_datatypes::page_value, false>;
};

template <>
class SplitRecordAccessor<page_value_datatypes::page_value, false> : public RecordAccessor<SplitRecordAccessor<page_value_datatypes::page_value, false>, page_value_datatypes::page_value, false> {
public:
    static constexpr auto num_splits = SplitParams<page_value_datatypes::page_value, false>::num_splits;

    SplitRecordAccessor() = default;
    SplitRecordAccessor(const std::array<void*, num_splits>& vptrs) : vptr_0_(reinterpret_cast<page_value_datatypes::page_value*>(vptrs[0])) {}

    operator bool() const {
        return vptr_0_ != nullptr;
    }
private:
    const int32_t& page_id_impl() const {
    return vptr_0_->page_id;
    }

    const int32_t& likes_impl() const {
    return vptr_0_->likes;
    }

    void copy_into_impl(page_value_datatypes::page_value* dst) const {
        if (vptr_0_) {
            dst->page_id = vptr_0_->page_id;
            dst->likes = vptr_0_->likes;
        }
    }

    const page_value_datatypes::page_value* vptr_0_;
    friend RecordAccessor<SplitRecordAccessor<page_value_datatypes::page_value, false>, page_value_datatypes::page_value, false>;
};

template <>
struct SplitParams<page_value_datatypes::page_value, true> {
    using split_type_list = std::tuple<page_value_split_structs::page_value_cell0, page_value_split_structs::page_value_cell1>;
    using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
    static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

    static constexpr auto split_builder = std::make_tuple(
        [](const page_value_datatypes::page_value& in) -> page_value_split_structs::page_value_cell0 {
            page_value_split_structs::page_value_cell0 out;
            out.likes = in.likes;
            return out;
        },
        [](const page_value_datatypes::page_value& in) -> page_value_split_structs::page_value_cell1 {
            page_value_split_structs::page_value_cell1 out;
            out.page_id = in.page_id;
            return out;
        }
    );

    static constexpr auto split_merger = std::make_tuple(
        [](page_value_datatypes::page_value* out, const page_value_split_structs::page_value_cell0& in) -> void {
            out->likes = in.likes;
        },
        [](page_value_datatypes::page_value* out, const page_value_split_structs::page_value_cell1& in) -> void {
            out->page_id = in.page_id;
        }
    );

    static constexpr auto map = [](int col_n) -> int {
        switch (col_n) {
        case 0: 
            return 1;
        default:
            break;
        }
        return 0;
    };
};

template <typename Accessor>
class RecordAccessor<Accessor, page_value_datatypes::page_value, true> {
public:
    const int32_t& page_id() const {
        return impl().page_id_impl();
    }

    const int32_t& likes() const {
        return impl().likes_impl();
    }

    void copy_into(page_value_datatypes::page_value* dst) const {
        return impl().copy_into_impl(dst);
    }

private:
    const Accessor& impl() const {
        return *static_cast<const Accessor*>(this);
    }
};

template <>
class UniRecordAccessor<page_value_datatypes::page_value, true> : public RecordAccessor<UniRecordAccessor<page_value_datatypes::page_value, true>, page_value_datatypes::page_value, true> {
public:
    UniRecordAccessor() = default;
    UniRecordAccessor(const page_value_datatypes::page_value* const vptr) : vptr_(vptr) {}

    operator bool() const {
        return vptr_ != nullptr;
    }

private:
    const int32_t& page_id_impl() const {
        return vptr_->page_id;
    }

    const int32_t& likes_impl() const {
        return vptr_->likes;
    }

    void copy_into_impl(page_value_datatypes::page_value* dst) const {
        if (vptr_) {
            dst->likes = vptr_->likes;
        }
        if (vptr_) {
            dst->page_id = vptr_->page_id;
        }
    }

    /*
    void copy_into_impl(page_value_datatypes::page_value* dst) const {
        if (vptr_) {
            memcpy(dst, vptr_, sizeof *dst);
        }
    }
    */

    const page_value_datatypes::page_value* vptr_;
    friend RecordAccessor<UniRecordAccessor<page_value_datatypes::page_value, true>, page_value_datatypes::page_value, true>;
};

template <>
class SplitRecordAccessor<page_value_datatypes::page_value, true> : public RecordAccessor<SplitRecordAccessor<page_value_datatypes::page_value, true>, page_value_datatypes::page_value, true> {
public:
    static constexpr auto num_splits = SplitParams<page_value_datatypes::page_value, true>::num_splits;

    SplitRecordAccessor() = default;
    SplitRecordAccessor(const std::array<void*, num_splits>& vptrs) : vptr_0_(reinterpret_cast<page_value_split_structs::page_value_cell0*>(vptrs[0])), vptr_1_(reinterpret_cast<page_value_split_structs::page_value_cell1*>(vptrs[1])) {}

    operator bool() const {
        return vptr_0_ != nullptr && vptr_1_ != nullptr;
    }
private:
    const int32_t& page_id_impl() const {
    return vptr_1_->page_id;
    }

    const int32_t& likes_impl() const {
    return vptr_0_->likes;
    }

    void copy_into_impl(page_value_datatypes::page_value* dst) const {
        if (vptr_0_) {
            dst->likes = vptr_0_->likes;
        }
        if (vptr_1_) {
            dst->page_id = vptr_1_->page_id;
        }
    }

    const page_value_split_structs::page_value_cell0* vptr_0_;
    const page_value_split_structs::page_value_cell1* vptr_1_;
    friend RecordAccessor<SplitRecordAccessor<page_value_datatypes::page_value, true>, page_value_datatypes::page_value, true>;
};

};  // namespace bench

namespace like_value_split_structs {

struct like_value_cell0 {
    using NamedColumn = typename like_value_datatypes::like_value::NamedColumn;

    int32_t user_id;
    int32_t page_id;
};

struct like_value_cell1 {
    using NamedColumn = typename like_value_datatypes::like_value::NamedColumn;

};

};  // namespace like_value_split_structs

namespace bench {
template <>
struct SplitParams<like_value_datatypes::like_value, false> {
    using split_type_list = std::tuple<like_value_datatypes::like_value>;
    using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
    static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

    static constexpr auto split_builder = std::make_tuple(
        [](const like_value_datatypes::like_value& in) -> like_value_datatypes::like_value {
            like_value_datatypes::like_value out;
            out.user_id = in.user_id;
            out.page_id = in.page_id;
            return out;
        }
    );

    static constexpr auto split_merger = std::make_tuple(
        [](like_value_datatypes::like_value* out, const like_value_datatypes::like_value& in) -> void {
            out->user_id = in.user_id;
            out->page_id = in.page_id;
        }
    );

    static constexpr auto map = [](int) -> int {
        return 0;
    };
};

template <typename Accessor>
class RecordAccessor<Accessor, like_value_datatypes::like_value, false> {
public:
    const int32_t& user_id() const {
        return impl().user_id_impl();
    }

    const int32_t& page_id() const {
        return impl().page_id_impl();
    }

    void copy_into(like_value_datatypes::like_value* dst) const {
        return impl().copy_into_impl(dst);
    }

private:
    const Accessor& impl() const {
        return *static_cast<const Accessor*>(this);
    }
};

template <>
class UniRecordAccessor<like_value_datatypes::like_value, false> : public RecordAccessor<UniRecordAccessor<like_value_datatypes::like_value, false>, like_value_datatypes::like_value, false> {
public:
    UniRecordAccessor() = default;
    UniRecordAccessor(const like_value_datatypes::like_value* const vptr) : vptr_(vptr) {}

    operator bool() const {
        return vptr_ != nullptr;
    }

private:
    const int32_t& user_id_impl() const {
        return vptr_->user_id;
    }

    const int32_t& page_id_impl() const {
        return vptr_->page_id;
    }

    void copy_into_impl(like_value_datatypes::like_value* dst) const {
        if (vptr_) {
            dst->user_id = vptr_->user_id;
            dst->page_id = vptr_->page_id;
        }
    }

    /*
    void copy_into_impl(like_value_datatypes::like_value* dst) const {
        if (vptr_) {
            memcpy(dst, vptr_, sizeof *dst);
        }
    }
    */

    const like_value_datatypes::like_value* vptr_;
    friend RecordAccessor<UniRecordAccessor<like_value_datatypes::like_value, false>, like_value_datatypes::like_value, false>;
};

template <>
class SplitRecordAccessor<like_value_datatypes::like_value, false> : public RecordAccessor<SplitRecordAccessor<like_value_datatypes::like_value, false>, like_value_datatypes::like_value, false> {
public:
    static constexpr auto num_splits = SplitParams<like_value_datatypes::like_value, false>::num_splits;

    SplitRecordAccessor() = default;
    SplitRecordAccessor(const std::array<void*, num_splits>& vptrs) : vptr_0_(reinterpret_cast<like_value_datatypes::like_value*>(vptrs[0])) {}

    operator bool() const {
        return vptr_0_ != nullptr;
    }
private:
    const int32_t& user_id_impl() const {
    return vptr_0_->user_id;
    }

    const int32_t& page_id_impl() const {
    return vptr_0_->page_id;
    }

    void copy_into_impl(like_value_datatypes::like_value* dst) const {
        if (vptr_0_) {
            dst->user_id = vptr_0_->user_id;
            dst->page_id = vptr_0_->page_id;
        }
    }

    const like_value_datatypes::like_value* vptr_0_;
    friend RecordAccessor<SplitRecordAccessor<like_value_datatypes::like_value, false>, like_value_datatypes::like_value, false>;
};

template <>
struct SplitParams<like_value_datatypes::like_value, true> {
    using split_type_list = std::tuple<like_value_datatypes::like_value>;
    using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
    static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

    static constexpr auto split_builder = std::make_tuple(
        [](const like_value_datatypes::like_value& in) -> like_value_datatypes::like_value {
            like_value_datatypes::like_value out;
            out.user_id = in.user_id;
            out.page_id = in.page_id;
            return out;
        }
    );

    static constexpr auto split_merger = std::make_tuple(
        [](like_value_datatypes::like_value* out, const like_value_datatypes::like_value& in) -> void {
            out->user_id = in.user_id;
            out->page_id = in.page_id;
        }
    );

    static constexpr auto map = [](int) -> int {
        return 0;
    };
};

template <typename Accessor>
class RecordAccessor<Accessor, like_value_datatypes::like_value, true> {
public:
    const int32_t& user_id() const {
        return impl().user_id_impl();
    }

    const int32_t& page_id() const {
        return impl().page_id_impl();
    }

    void copy_into(like_value_datatypes::like_value* dst) const {
        return impl().copy_into_impl(dst);
    }

private:
    const Accessor& impl() const {
        return *static_cast<const Accessor*>(this);
    }
};

template <>
class UniRecordAccessor<like_value_datatypes::like_value, true> : public RecordAccessor<UniRecordAccessor<like_value_datatypes::like_value, true>, like_value_datatypes::like_value, true> {
public:
    UniRecordAccessor() = default;
    UniRecordAccessor(const like_value_datatypes::like_value* const vptr) : vptr_(vptr) {}

    operator bool() const {
        return vptr_ != nullptr;
    }

private:
    const int32_t& user_id_impl() const {
        return vptr_->user_id;
    }

    const int32_t& page_id_impl() const {
        return vptr_->page_id;
    }

    void copy_into_impl(like_value_datatypes::like_value* dst) const {
        if (vptr_) {
            dst->user_id = vptr_->user_id;
            dst->page_id = vptr_->page_id;
        }
    }

    /*
    void copy_into_impl(like_value_datatypes::like_value* dst) const {
        if (vptr_) {
            memcpy(dst, vptr_, sizeof *dst);
        }
    }
    */

    const like_value_datatypes::like_value* vptr_;
    friend RecordAccessor<UniRecordAccessor<like_value_datatypes::like_value, true>, like_value_datatypes::like_value, true>;
};

template <>
class SplitRecordAccessor<like_value_datatypes::like_value, true> : public RecordAccessor<SplitRecordAccessor<like_value_datatypes::like_value, true>, like_value_datatypes::like_value, true> {
public:
    static constexpr auto num_splits = SplitParams<like_value_datatypes::like_value, true>::num_splits;

    SplitRecordAccessor() = default;
    SplitRecordAccessor(const std::array<void*, num_splits>& vptrs) : vptr_0_(reinterpret_cast<like_value_datatypes::like_value*>(vptrs[0])) {}

    operator bool() const {
        return vptr_0_ != nullptr;
    }
private:
    const int32_t& user_id_impl() const {
    return vptr_0_->user_id;
    }

    const int32_t& page_id_impl() const {
    return vptr_0_->page_id;
    }

    void copy_into_impl(like_value_datatypes::like_value* dst) const {
        if (vptr_0_) {
            dst->user_id = vptr_0_->user_id;
            dst->page_id = vptr_0_->page_id;
        }
    }

    const like_value_datatypes::like_value* vptr_0_;
    friend RecordAccessor<SplitRecordAccessor<like_value_datatypes::like_value, true>, like_value_datatypes::like_value, true>;
};

};  // namespace bench

