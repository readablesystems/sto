#pragma once

#include <bitset>

namespace sto {

namespace adapter {

template <typename T>
struct Stats {
    using NamedColumn = typename T::NamedColumn;
    static constexpr std::underlying_type_t<NamedColumn> Columns =
        static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
    std::bitset<Columns> read_data = {};
    std::bitset<Columns> write_data = {};
};

};  // namespace adapter

};  // namespace sto
