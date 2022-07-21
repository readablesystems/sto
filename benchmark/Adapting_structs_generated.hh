#pragma once

#include <type_traits>
//#include "AdapterStructs.hh"

namespace adapting_bench {

namespace adapting_value_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct adapting_value {
    using RecordAccessor = adapting_value_datatypes::RecordAccessor;
    using NamedColumn = adapting_value_datatypes::NamedColumn;

    int64_t read_only;
    int64_t write_some;
    int64_t write_much;
    int64_t write_most;
};

enum class NamedColumn : int {
    read_only = 0,
    write_some,
    write_much,
    write_most,
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
    if constexpr (Column < NamedColumn::write_some) {
        return NamedColumn::read_only;
    }
    if constexpr (Column < NamedColumn::write_much) {
        return NamedColumn::write_some;
    }
    if constexpr (Column < NamedColumn::write_most) {
        return NamedColumn::write_much;
    }
    return NamedColumn::write_most;
}

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::read_only> {
    using NamedColumn = adapting_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int64_t;
    using value_type = int64_t;
    static constexpr NamedColumn Column = NamedColumn::read_only;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::write_some> {
    using NamedColumn = adapting_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int64_t;
    using value_type = int64_t;
    static constexpr NamedColumn Column = NamedColumn::write_some;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::write_much> {
    using NamedColumn = adapting_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int64_t;
    using value_type = int64_t;
    static constexpr NamedColumn Column = NamedColumn::write_much;
    static constexpr bool is_array = false;
};

template <>
struct accessor_info<NamedColumn::write_most> {
    using NamedColumn = adapting_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = int64_t;
    using value_type = int64_t;
    static constexpr NamedColumn Column = NamedColumn::write_most;
    static constexpr bool is_array = false;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

struct StructAccessor {
    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            adapting_value* ptr) {
        if constexpr (NamedColumn::read_only <= Column && Column < NamedColumn::write_some) {
            return ptr->read_only;
        }
        if constexpr (NamedColumn::write_some <= Column && Column < NamedColumn::write_much) {
            return ptr->write_some;
        }
        if constexpr (NamedColumn::write_much <= Column && Column < NamedColumn::write_most) {
            return ptr->write_much;
        }
        if constexpr (NamedColumn::write_most <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->write_most;
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
    inline static constexpr void copy_cell(adapting_value* dest, adapting_value* src) {
        if constexpr(Cell == 0) {
            *dest = *src;
        }
        (void) dest; (void) src;
    }
};

template <>
struct SplitPolicy<1> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 1, 1, 1, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 1) {
            return 3;
        }
        if (cell == 0) {
            return 1;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(adapting_value* dest, adapting_value* src) {
        if constexpr(Cell == 1) {
            dest->read_only = src->read_only;
            dest->write_some = src->write_some;
            dest->write_much = src->write_much;
        }
        if constexpr(Cell == 0) {
            dest->write_most = src->write_most;
        }
        (void) dest; (void) src;
    }
};

template <>
struct SplitPolicy<2> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 1, 1, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 1) {
            return 2;
        }
        if (cell == 0) {
            return 2;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(adapting_value* dest, adapting_value* src) {
        if constexpr(Cell == 1) {
            dest->read_only = src->read_only;
            dest->write_some = src->write_some;
        }
        if constexpr(Cell == 0) {
            dest->write_much = src->write_much;
            dest->write_most = src->write_most;
        }
        (void) dest; (void) src;
    }
};

template <>
struct SplitPolicy<3> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 1, 0, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
    inline static constexpr size_t cell_col_count(int cell) {
        if (cell == 1) {
            return 1;
        }
        if (cell == 0) {
            return 3;
        }
        return 0;
    }
    template <int Cell>
    inline static constexpr void copy_cell(adapting_value* dest, adapting_value* src) {
        if constexpr(Cell == 1) {
            dest->read_only = src->read_only;
        }
        if constexpr(Cell == 0) {
            dest->write_some = src->write_some;
            dest->write_much = src->write_much;
            dest->write_most = src->write_most;
        }
        (void) dest; (void) src;
    }
};

class RecordAccessor {
public:
    using NamedColumn = adapting_value_datatypes::NamedColumn;
    //using SplitTable = adapting_value_datatypes::SplitTable;
    using SplitType = adapting_value_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<adapting_value>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = adapting_value;
    static constexpr auto DEFAULT_SPLIT = 0;
    static constexpr auto MAX_SPLITS = 2;
    static constexpr auto MAX_POINTERS = MAX_SPLITS;
    static constexpr auto POLICIES = 4;

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
        if constexpr (Index == 2) {
            return SplitPolicy<2>::column_to_cell(column);
        }
        if constexpr (Index == 3) {
            return SplitPolicy<3>::column_to_cell(column);
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
        if (index == 2) {
            return SplitPolicy<2>::column_to_cell(column);
        }
        if (index == 3) {
            return SplitPolicy<3>::column_to_cell(column);
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
        if (index == 2) {
            copy_cell<2>(cell, dest, src);
            return;
        }
        if (index == 3) {
            copy_cell<3>(cell, dest, src);
            return;
        }
    }

    template <int Cell>
    inline static constexpr void copy_split_cell(int index, ValueType* dest, ValueType* src) {
        if (index == 0) {
            SplitPolicy<0>::copy_cell<Cell>(dest, src);
            return;
        }
        if (index == 1) {
            SplitPolicy<1>::copy_cell<Cell>(dest, src);
            return;
        }
        if (index == 2) {
            SplitPolicy<2>::copy_cell<Cell>(dest, src);
            return;
        }
        if (index == 3) {
            SplitPolicy<3>::copy_cell<Cell>(dest, src);
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
        if (index == 2) {
            return SplitPolicy<2>::cell_col_count(cell);
        }
        if (index == 3) {
            return SplitPolicy<3>::cell_col_count(cell);
        }
        return 0;
    }

    inline void copy_into(adapting_value* vptr, int index) {
        if (vptrs_[0]) {
            copy_split_cell<0>(index, vptr, vptrs_[0]);
        }
        if (vptrs_[1]) {
            copy_split_cell<1>(index, vptr, vptrs_[1]);
        }
    }

    inline void copy_into(adapting_value* vptr) {
        copy_into(vptr, splitindex_);
    }
    inline typename accessor_info<NamedColumn::read_only>::value_type& read_only() {
        return vptrs_[cell_of(NamedColumn::read_only)]->read_only;
    }

    inline const typename accessor_info<NamedColumn::read_only>::value_type& read_only() const {
        return vptrs_[cell_of(NamedColumn::read_only)]->read_only;
    }

    inline typename accessor_info<NamedColumn::write_some>::value_type& write_some() {
        return vptrs_[cell_of(NamedColumn::write_some)]->write_some;
    }

    inline const typename accessor_info<NamedColumn::write_some>::value_type& write_some() const {
        return vptrs_[cell_of(NamedColumn::write_some)]->write_some;
    }

    inline typename accessor_info<NamedColumn::write_much>::value_type& write_much() {
        return vptrs_[cell_of(NamedColumn::write_much)]->write_much;
    }

    inline const typename accessor_info<NamedColumn::write_much>::value_type& write_much() const {
        return vptrs_[cell_of(NamedColumn::write_much)]->write_much;
    }

    inline typename accessor_info<NamedColumn::write_most>::value_type& write_most() {
        return vptrs_[cell_of(NamedColumn::write_most)]->write_most;
    }

    inline const typename accessor_info<NamedColumn::write_most>::value_type& write_most() const {
        return vptrs_[cell_of(NamedColumn::write_most)]->write_most;
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::read_only <= Column && Column < NamedColumn::write_some) {
            return vptrs_[cell_of(NamedColumn::read_only)]->read_only;
        }
        if constexpr (NamedColumn::write_some <= Column && Column < NamedColumn::write_much) {
            return vptrs_[cell_of(NamedColumn::write_some)]->write_some;
        }
        if constexpr (NamedColumn::write_much <= Column && Column < NamedColumn::write_most) {
            return vptrs_[cell_of(NamedColumn::write_much)]->write_much;
        }
        if constexpr (NamedColumn::write_most <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::write_most)]->write_most;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }


    template <NamedColumn Column, typename... Args>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            Args&&... args) {
        return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
    }

    std::array<adapting_value*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace adapting_value_datatypes

using adapting_value = adapting_value_datatypes::adapting_value;

};  // namespace adapting_bench

namespace adapting_bench {

namespace adapting_value_split_structs {

struct adapting_value_cell0 {
    using NamedColumn = typename ::adapting_bench::adapting_value::NamedColumn;

    int64_t write_most;
};

struct adapting_value_cell1 {
    using NamedColumn = typename ::adapting_bench::adapting_value::NamedColumn;

    int64_t read_only;
    int64_t write_some;
    int64_t write_much;
};

};  // namespace adapting_value_split_structs

using adapting_value_cell0 = adapting_value_split_structs::adapting_value_cell0;
using adapting_value_cell1 = adapting_value_split_structs::adapting_value_cell1;

};  // namespace adapting_bench

namespace bench {
template <>
struct SplitParams<::adapting_bench::adapting_value, false> {
    using split_type_list = std::tuple<::adapting_bench::adapting_value>;
    using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
    static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

    static constexpr auto split_builder = std::make_tuple(
        [] (const ::adapting_bench::adapting_value& in) -> ::adapting_bench::adapting_value {
            ::adapting_bench::adapting_value out;
            out.read_only = in.read_only;
            out.write_some = in.write_some;
            out.write_much = in.write_much;
            out.write_most = in.write_most;
            return out;
        }
    );

    static constexpr auto split_merger = std::make_tuple(
        [] (::adapting_bench::adapting_value* out, const ::adapting_bench::adapting_value& in) -> void {
            out->read_only = in.read_only;
            out->write_some = in.write_some;
            out->write_much = in.write_much;
            out->write_most = in.write_most;
        }
    );

    static constexpr auto map = [] (int) -> int {
        return 0;
    };
};

template <typename Accessor>
class RecordAccessor<Accessor, ::adapting_bench::adapting_value, false> {
public:
    const int64_t& read_only() const {
        return impl().read_only_impl();
    }

    const int64_t& write_some() const {
        return impl().write_some_impl();
    }

    const int64_t& write_much() const {
        return impl().write_much_impl();
    }

    const int64_t& write_most() const {
        return impl().write_most_impl();
    }

    void copy_into(::adapting_bench::adapting_value* dst) const {
        return impl().copy_into_impl(dst);
    }

private:
    const Accessor& impl() const {
        return *static_cast<const Accessor*>(this);
    }
};

template <>
class UniRecordAccessor<::adapting_bench::adapting_value, false> : public RecordAccessor<UniRecordAccessor<::adapting_bench::adapting_value, false>, ::adapting_bench::adapting_value, false> {
public:
    UniRecordAccessor() = default;
    UniRecordAccessor(const ::adapting_bench::adapting_value* const vptr) : vptr_(vptr) {}

    operator bool() const {
        return vptr_ != nullptr;
    }

private:
    const int64_t& read_only_impl() const {
        return vptr_->read_only;
    }

    const int64_t& write_some_impl() const {
        return vptr_->write_some;
    }

    const int64_t& write_much_impl() const {
        return vptr_->write_much;
    }

    const int64_t& write_most_impl() const {
        return vptr_->write_most;
    }

    void copy_into_impl(::adapting_bench::adapting_value* dst) const {
        if (vptr_) {
            dst->read_only = vptr_->read_only;
            dst->write_some = vptr_->write_some;
            dst->write_much = vptr_->write_much;
            dst->write_most = vptr_->write_most;
        }
    }

    const ::adapting_bench::adapting_value* vptr_;
    friend RecordAccessor<UniRecordAccessor<::adapting_bench::adapting_value, false>, ::adapting_bench::adapting_value, false>;
};

template <>
class SplitRecordAccessor<::adapting_bench::adapting_value, false> : public RecordAccessor<SplitRecordAccessor<::adapting_bench::adapting_value, false>, ::adapting_bench::adapting_value, false> {
public:
    static constexpr auto num_splits = SplitParams<::adapting_bench::adapting_value, false>::num_splits;

    SplitRecordAccessor() = default;
    SplitRecordAccessor(const std::array<void*, num_splits>& vptrs) : vptr_0_(reinterpret_cast<::adapting_bench::adapting_value*>(vptrs[0])) {}

    operator bool() const {
        return vptr_0_ != nullptr;
    }
private:
    const int64_t& read_only_impl() const {
    return vptr_0_->read_only;
    }

    const int64_t& write_some_impl() const {
    return vptr_0_->write_some;
    }

    const int64_t& write_much_impl() const {
    return vptr_0_->write_much;
    }

    const int64_t& write_most_impl() const {
    return vptr_0_->write_most;
    }

    void copy_into_impl(::adapting_bench::adapting_value* dst) const {
        if (vptr_0_) {
            dst->read_only = vptr_0_->read_only;
            dst->write_some = vptr_0_->write_some;
            dst->write_much = vptr_0_->write_much;
            dst->write_most = vptr_0_->write_most;
        }
    }

    const ::adapting_bench::adapting_value* vptr_0_;
    friend RecordAccessor<SplitRecordAccessor<::adapting_bench::adapting_value, false>, ::adapting_bench::adapting_value, false>;
};

template <>
struct SplitParams<::adapting_bench::adapting_value, true> {
    using split_type_list = std::tuple<::adapting_bench::adapting_value_cell0, ::adapting_bench::adapting_value_cell1>;
    using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
    static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

    static constexpr auto split_builder = std::make_tuple(
        [] (const ::adapting_bench::adapting_value& in) -> ::adapting_bench::adapting_value_cell0 {
            ::adapting_bench::adapting_value_cell0 out;
            out.write_most = in.write_most;
            return out;
        },
        [] (const ::adapting_bench::adapting_value& in) -> ::adapting_bench::adapting_value_cell1 {
            ::adapting_bench::adapting_value_cell1 out;
            out.read_only = in.read_only;
            out.write_some = in.write_some;
            out.write_much = in.write_much;
            return out;
        }
    );

    static constexpr auto split_merger = std::make_tuple(
        [] (::adapting_bench::adapting_value* out, const ::adapting_bench::adapting_value_cell0& in) -> void {
            out->write_most = in.write_most;
        },
        [] (::adapting_bench::adapting_value* out, const ::adapting_bench::adapting_value_cell1& in) -> void {
            out->read_only = in.read_only;
            out->write_some = in.write_some;
            out->write_much = in.write_much;
        }
    );

    static constexpr auto map = [] (int col_n) -> int {
        switch (col_n) {
        case 0: case 1: case 2: 
            return 1;
        default:
            break;
        }
        return 0;
    };
};

template <typename Accessor>
class RecordAccessor<Accessor, ::adapting_bench::adapting_value, true> {
public:
    const int64_t& read_only() const {
        return impl().read_only_impl();
    }

    const int64_t& write_some() const {
        return impl().write_some_impl();
    }

    const int64_t& write_much() const {
        return impl().write_much_impl();
    }

    const int64_t& write_most() const {
        return impl().write_most_impl();
    }

    void copy_into(::adapting_bench::adapting_value* dst) const {
        return impl().copy_into_impl(dst);
    }

private:
    const Accessor& impl() const {
        return *static_cast<const Accessor*>(this);
    }
};

template <>
class UniRecordAccessor<::adapting_bench::adapting_value, true> : public RecordAccessor<UniRecordAccessor<::adapting_bench::adapting_value, true>, ::adapting_bench::adapting_value, true> {
public:
    UniRecordAccessor() = default;
    UniRecordAccessor(const ::adapting_bench::adapting_value* const vptr) : vptr_(vptr) {}

    operator bool() const {
        return vptr_ != nullptr;
    }

private:
    const int64_t& read_only_impl() const {
        return vptr_->read_only;
    }

    const int64_t& write_some_impl() const {
        return vptr_->write_some;
    }

    const int64_t& write_much_impl() const {
        return vptr_->write_much;
    }

    const int64_t& write_most_impl() const {
        return vptr_->write_most;
    }

    void copy_into_impl(::adapting_bench::adapting_value* dst) const {
        if (vptr_) {
            dst->write_most = vptr_->write_most;
        }
        if (vptr_) {
            dst->read_only = vptr_->read_only;
            dst->write_some = vptr_->write_some;
            dst->write_much = vptr_->write_much;
        }
    }

    const ::adapting_bench::adapting_value* vptr_;
    friend RecordAccessor<UniRecordAccessor<::adapting_bench::adapting_value, true>, ::adapting_bench::adapting_value, true>;
};

template <>
class SplitRecordAccessor<::adapting_bench::adapting_value, true> : public RecordAccessor<SplitRecordAccessor<::adapting_bench::adapting_value, true>, ::adapting_bench::adapting_value, true> {
public:
    static constexpr auto num_splits = SplitParams<::adapting_bench::adapting_value, true>::num_splits;

    SplitRecordAccessor() = default;
    SplitRecordAccessor(const std::array<void*, num_splits>& vptrs) : vptr_0_(reinterpret_cast<::adapting_bench::adapting_value_cell0*>(vptrs[0])), vptr_1_(reinterpret_cast<::adapting_bench::adapting_value_cell1*>(vptrs[1])) {}

    operator bool() const {
        return vptr_0_ != nullptr && vptr_1_ != nullptr;
    }
private:
    const int64_t& read_only_impl() const {
    return vptr_1_->read_only;
    }

    const int64_t& write_some_impl() const {
    return vptr_1_->write_some;
    }

    const int64_t& write_much_impl() const {
    return vptr_1_->write_much;
    }

    const int64_t& write_most_impl() const {
    return vptr_0_->write_most;
    }

    void copy_into_impl(::adapting_bench::adapting_value* dst) const {
        if (vptr_0_) {
            dst->write_most = vptr_0_->write_most;
        }
        if (vptr_1_) {
            dst->read_only = vptr_1_->read_only;
            dst->write_some = vptr_1_->write_some;
            dst->write_much = vptr_1_->write_much;
        }
    }

    const ::adapting_bench::adapting_value_cell0* vptr_0_;
    const ::adapting_bench::adapting_value_cell1* vptr_1_;
    friend RecordAccessor<SplitRecordAccessor<::adapting_bench::adapting_value, true>, ::adapting_bench::adapting_value, true>;
};

};  // namespace bench

