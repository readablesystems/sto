#pragma once

#include "Adapter.hh"

namespace sto {

namespace adapter {

template <typename InfoType>
struct Accessor {
    using NamedColumn = typename InfoType::NamedColumn;
    using type = typename InfoType::type;
    using value_type = typename InfoType::value_type;

    using adapter_type = GlobalAdapter<type, NamedColumn>;

    static constexpr auto Column = InfoType::Column;

    Accessor() = default;
    template <typename... Args>
    explicit Accessor(Args&&... args) : value_(std::forward<Args>(args)...) {}

    operator const value_type() const {
        if constexpr (InfoType::is_array) {
            adapter_type::CountReads(Column, Column + value_.size());
        } else {
            adapter_type::CountRead(Column);
        }
        return value_;
    }

    value_type operator =(const value_type& other) {
        if constexpr (InfoType::is_array) {
            adapter_type::CountWrites(Column, Column + value_.size());
        } else {
            adapter_type::CountWrite(Column);
        }
        return value_ = other;
    }

    value_type operator =(Accessor<InfoType>& other) {
        if constexpr (InfoType::is_array) {
            adapter_type::CountReads(Column, Column + other.value_.size());
            adapter_type::CountWrites(Column, Column + value_.size());
        } else {
            adapter_type::CountRead(Column);
            adapter_type::CountWrite(Column);
        }
        return value_ = other.value_;
    }

    template <typename OtherInfoType>
    value_type operator =(const Accessor<OtherInfoType>& other) {
        if constexpr (InfoType::is_array && OtherInfoType::is_array) {
            adapter_type::CountReads(OtherInfoType::Column, OtherInfoType::Column + other.value_.size());
            adapter_type::CountWrites(Column, Column + value_.size());
        } else {
            adapter_type::CountRead(OtherInfoType::Column);
            adapter_type::CountWrite(Column);
        }
        return value_ = other.value_;
    }

    template <bool is_array = InfoType::is_array>
    std::enable_if_t<is_array, void>
    operator ()(const std::underlying_type_t<NamedColumn>& index, const type& value) {
        adapter_type::CountWrite(Column + index);
        value_[index] = value;
    }

    template <bool is_array = InfoType::is_array>
    std::enable_if_t<is_array, type&>
    operator ()(const std::underlying_type_t<NamedColumn>& index) {
        adapter_type::CountWrite(Column + index);
        return value_[index];
    }

    template <bool is_array = InfoType::is_array>
    std::enable_if_t<is_array, type>
    operator [](const std::underlying_type_t<NamedColumn>& index) {
        adapter_type::CountRead(Column + index);
        return value_[index];
    }

    template <bool is_array = InfoType::is_array>
    std::enable_if_t<is_array, const type&>
    operator [](const std::underlying_type_t<NamedColumn>& index) const {
        adapter_type::CountRead(Column + index);
        return value_[index];
    }

    template <bool is_array = InfoType::is_array>
    std::enable_if_t<!is_array, type&>
    operator *() {
        adapter_type::CountWrite(Column);
        return value_;
    }

    template <bool is_array = InfoType::is_array>
    std::enable_if_t<!is_array, type*>
    operator ->() {
        adapter_type::CountWrite(Column);
        return &value_;
    }

    value_type value_;
};

}  // namespace adapter

}  // namespace sto
