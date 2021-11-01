#pragma once

#include <type_traits>
//#include "AdapterStructs.hh"

namespace ordered_value_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct ordered_value {
    using RecordAccessor = ordered_value_datatypes::RecordAccessor;
    using NamedColumn = ordered_value_datatypes::NamedColumn;

    std::array<uint64_t, 2> ro;
    std::array<uint64_t, 8> rw;
    std::array<uint64_t, 2> wo;
};

enum class NamedColumn : int {
    ro = 0,
    rw = 2,
    wo = 10,
    COLCOUNT = 12
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
    if constexpr (Column < NamedColumn::rw) {
        return NamedColumn::ro;
    }
    if constexpr (Column < NamedColumn::wo) {
        return NamedColumn::rw;
    }
    return NamedColumn::wo;
}

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
};

template <>
struct SplitPolicy<1> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
};

template <>
struct SplitPolicy<2> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
};

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::ro> {
    using NamedColumn = ordered_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint64_t;
    using value_type = std::array<uint64_t, 2>;
    static constexpr NamedColumn Column = NamedColumn::ro;
    static constexpr bool is_array = true;
};

template <>
struct accessor_info<NamedColumn::rw> {
    using NamedColumn = ordered_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint64_t;
    using value_type = std::array<uint64_t, 8>;
    static constexpr NamedColumn Column = NamedColumn::rw;
    static constexpr bool is_array = true;
};

template <>
struct accessor_info<NamedColumn::wo> {
    using NamedColumn = ordered_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint64_t;
    using value_type = std::array<uint64_t, 2>;
    static constexpr NamedColumn Column = NamedColumn::wo;
    static constexpr bool is_array = true;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

class RecordAccessor {
public:
    using NamedColumn = ordered_value_datatypes::NamedColumn;
    //using SplitTable = ordered_value_datatypes::SplitTable;
    using SplitType = ordered_value_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<ordered_value>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = ordered_value;
    static constexpr auto DEFAULT_SPLIT = 0;
    static constexpr auto MAX_SPLITS = 2;
    static constexpr auto MAX_POINTERS = MAX_SPLITS;
    static constexpr auto POLICIES = 3;

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
        return 0;
    }

    const auto cell_of(NamedColumn column) const {
        return split_of(splitindex_, column);
    }

    void copy_into(ordered_value* vptr) {
        memcpy(vptr, vptrs_[0], sizeof *vptr);
    }

    inline typename accessor_info<NamedColumn::ro>::value_type& ro() {
        return vptrs_[cell_of(NamedColumn::ro)]->ro;
    }

    inline typename accessor_info<NamedColumn::ro>::type& ro(
            const std::underlying_type_t<NamedColumn> index) {
        return vptrs_[cell_of(NamedColumn::ro)]->ro[index];
    }

    inline const typename accessor_info<NamedColumn::ro>::value_type& ro() const {
        return vptrs_[cell_of(NamedColumn::ro)]->ro;
    }

    inline const typename accessor_info<NamedColumn::ro>::type& ro(
            const std::underlying_type_t<NamedColumn> index) const {
        return vptrs_[cell_of(NamedColumn::ro)]->ro[index];
    }

    inline typename accessor_info<NamedColumn::rw>::value_type& rw() {
        return vptrs_[cell_of(NamedColumn::rw)]->rw;
    }

    inline typename accessor_info<NamedColumn::rw>::type& rw(
            const std::underlying_type_t<NamedColumn> index) {
        return vptrs_[cell_of(NamedColumn::rw)]->rw[index];
    }

    inline const typename accessor_info<NamedColumn::rw>::value_type& rw() const {
        return vptrs_[cell_of(NamedColumn::rw)]->rw;
    }

    inline const typename accessor_info<NamedColumn::rw>::type& rw(
            const std::underlying_type_t<NamedColumn> index) const {
        return vptrs_[cell_of(NamedColumn::rw)]->rw[index];
    }

    inline typename accessor_info<NamedColumn::wo>::value_type& wo() {
        return vptrs_[cell_of(NamedColumn::wo)]->wo;
    }

    inline typename accessor_info<NamedColumn::wo>::type& wo(
            const std::underlying_type_t<NamedColumn> index) {
        return vptrs_[cell_of(NamedColumn::wo)]->wo[index];
    }

    inline const typename accessor_info<NamedColumn::wo>::value_type& wo() const {
        return vptrs_[cell_of(NamedColumn::wo)]->wo;
    }

    inline const typename accessor_info<NamedColumn::wo>::type& wo(
            const std::underlying_type_t<NamedColumn> index) const {
        return vptrs_[cell_of(NamedColumn::wo)]->wo[index];
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::ro <= Column && Column < NamedColumn::rw) {
            return vptrs_[cell_of(NamedColumn::ro)]->ro;
        }
        if constexpr (NamedColumn::rw <= Column && Column < NamedColumn::wo) {
            return vptrs_[cell_of(NamedColumn::rw)]->rw;
        }
        if constexpr (NamedColumn::wo <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::wo)]->wo;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }

    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            ordered_value* ptr) {
        if constexpr (NamedColumn::ro <= Column && Column < NamedColumn::rw) {
            return ptr->ro[static_cast<std::underlying_type_t<NamedColumn>>(Column - NamedColumn::ro)];
        }
        if constexpr (NamedColumn::rw <= Column && Column < NamedColumn::wo) {
            return ptr->rw[static_cast<std::underlying_type_t<NamedColumn>>(Column - NamedColumn::rw)];
        }
        if constexpr (NamedColumn::wo <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->wo[static_cast<std::underlying_type_t<NamedColumn>>(Column - NamedColumn::wo)];
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }

    std::array<ordered_value*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace ordered_value_datatypes

using ordered_value = ordered_value_datatypes::ordered_value;

namespace unordered_value_datatypes {

enum class NamedColumn;

using SplitType = int;

class RecordAccessor;

struct unordered_value {
    using RecordAccessor = unordered_value_datatypes::RecordAccessor;
    using NamedColumn = unordered_value_datatypes::NamedColumn;

    std::array<uint64_t, 2> ro;
    std::array<uint64_t, 8> rw;
    std::array<uint64_t, 2> wo;
};

enum class NamedColumn : int {
    ro = 0,
    rw = 2,
    wo = 10,
    COLCOUNT = 12
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
    if constexpr (Column < NamedColumn::rw) {
        return NamedColumn::ro;
    }
    if constexpr (Column < NamedColumn::wo) {
        return NamedColumn::rw;
    }
    return NamedColumn::wo;
}

template <size_t Variant>
struct SplitPolicy;

template <>
struct SplitPolicy<0> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
};

template <>
struct SplitPolicy<1> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
};

template <>
struct SplitPolicy<2> {
    static constexpr auto ColCount = static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    static constexpr int policy[ColCount] = { 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    inline static constexpr int column_to_cell(NamedColumn column) {
        return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
    }
};

template <NamedColumn Column>
struct accessor_info;

template <>
struct accessor_info<NamedColumn::ro> {
    using NamedColumn = unordered_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint64_t;
    using value_type = std::array<uint64_t, 2>;
    static constexpr NamedColumn Column = NamedColumn::ro;
    static constexpr bool is_array = true;
};

template <>
struct accessor_info<NamedColumn::rw> {
    using NamedColumn = unordered_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint64_t;
    using value_type = std::array<uint64_t, 8>;
    static constexpr NamedColumn Column = NamedColumn::rw;
    static constexpr bool is_array = true;
};

template <>
struct accessor_info<NamedColumn::wo> {
    using NamedColumn = unordered_value_datatypes::NamedColumn;
    using struct_type = RecordAccessor;
    using type = uint64_t;
    using value_type = std::array<uint64_t, 2>;
    static constexpr NamedColumn Column = NamedColumn::wo;
    static constexpr bool is_array = true;
};

template <NamedColumn ColumnValue>
struct accessor_info : accessor_info<RoundedNamedColumn<ColumnValue>()> {
    static constexpr NamedColumn Column = ColumnValue;
};

class RecordAccessor {
public:
    using NamedColumn = unordered_value_datatypes::NamedColumn;
    //using SplitTable = unordered_value_datatypes::SplitTable;
    using SplitType = unordered_value_datatypes::SplitType;
    //using StatsType = ::sto::adapter::Stats<unordered_value>;
    //template <NamedColumn Column>
    //using ValueAccessor = ::sto::adapter::ValueAccessor<accessor_info<Column>>;
    using ValueType = unordered_value;
    static constexpr auto DEFAULT_SPLIT = 0;
    static constexpr auto MAX_SPLITS = 2;
    static constexpr auto MAX_POINTERS = MAX_SPLITS;
    static constexpr auto POLICIES = 3;

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
        return 0;
    }

    const auto cell_of(NamedColumn column) const {
        return split_of(splitindex_, column);
    }

    void copy_into(unordered_value* vptr) {
        memcpy(vptr, vptrs_[0], sizeof *vptr);
    }

    inline typename accessor_info<NamedColumn::ro>::value_type& ro() {
        return vptrs_[cell_of(NamedColumn::ro)]->ro;
    }

    inline typename accessor_info<NamedColumn::ro>::type& ro(
            const std::underlying_type_t<NamedColumn> index) {
        return vptrs_[cell_of(NamedColumn::ro)]->ro[index];
    }

    inline const typename accessor_info<NamedColumn::ro>::value_type& ro() const {
        return vptrs_[cell_of(NamedColumn::ro)]->ro;
    }

    inline const typename accessor_info<NamedColumn::ro>::type& ro(
            const std::underlying_type_t<NamedColumn> index) const {
        return vptrs_[cell_of(NamedColumn::ro)]->ro[index];
    }

    inline typename accessor_info<NamedColumn::rw>::value_type& rw() {
        return vptrs_[cell_of(NamedColumn::rw)]->rw;
    }

    inline typename accessor_info<NamedColumn::rw>::type& rw(
            const std::underlying_type_t<NamedColumn> index) {
        return vptrs_[cell_of(NamedColumn::rw)]->rw[index];
    }

    inline const typename accessor_info<NamedColumn::rw>::value_type& rw() const {
        return vptrs_[cell_of(NamedColumn::rw)]->rw;
    }

    inline const typename accessor_info<NamedColumn::rw>::type& rw(
            const std::underlying_type_t<NamedColumn> index) const {
        return vptrs_[cell_of(NamedColumn::rw)]->rw[index];
    }

    inline typename accessor_info<NamedColumn::wo>::value_type& wo() {
        return vptrs_[cell_of(NamedColumn::wo)]->wo;
    }

    inline typename accessor_info<NamedColumn::wo>::type& wo(
            const std::underlying_type_t<NamedColumn> index) {
        return vptrs_[cell_of(NamedColumn::wo)]->wo[index];
    }

    inline const typename accessor_info<NamedColumn::wo>::value_type& wo() const {
        return vptrs_[cell_of(NamedColumn::wo)]->wo;
    }

    inline const typename accessor_info<NamedColumn::wo>::type& wo(
            const std::underlying_type_t<NamedColumn> index) const {
        return vptrs_[cell_of(NamedColumn::wo)]->wo[index];
    }

    template <NamedColumn Column>
    inline typename accessor_info<RoundedNamedColumn<Column>()>::value_type& get_raw() {
        if constexpr (NamedColumn::ro <= Column && Column < NamedColumn::rw) {
            return vptrs_[cell_of(NamedColumn::ro)]->ro;
        }
        if constexpr (NamedColumn::rw <= Column && Column < NamedColumn::wo) {
            return vptrs_[cell_of(NamedColumn::rw)]->rw;
        }
        if constexpr (NamedColumn::wo <= Column && Column < NamedColumn::COLCOUNT) {
            return vptrs_[cell_of(NamedColumn::wo)]->wo;
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }

    template <NamedColumn Column>
    static inline typename accessor_info<RoundedNamedColumn<Column>()>::type& get_value(
            unordered_value* ptr) {
        if constexpr (NamedColumn::ro <= Column && Column < NamedColumn::rw) {
            return ptr->ro[static_cast<std::underlying_type_t<NamedColumn>>(Column - NamedColumn::ro)];
        }
        if constexpr (NamedColumn::rw <= Column && Column < NamedColumn::wo) {
            return ptr->rw[static_cast<std::underlying_type_t<NamedColumn>>(Column - NamedColumn::rw)];
        }
        if constexpr (NamedColumn::wo <= Column && Column < NamedColumn::COLCOUNT) {
            return ptr->wo[static_cast<std::underlying_type_t<NamedColumn>>(Column - NamedColumn::wo)];
        }
        static_assert(Column < NamedColumn::COLCOUNT);
    }

    std::array<unordered_value*, MAX_POINTERS> vptrs_ = { nullptr, };
    SplitType splitindex_ = {};
};

};  // namespace unordered_value_datatypes

using unordered_value = unordered_value_datatypes::unordered_value;

