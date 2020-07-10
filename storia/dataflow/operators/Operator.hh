#pragma once

#include <optional>

#include "../StateEntry.hh"
#include "../StateUpdate.hh"

namespace storia {

template <typename T, typename U=T>
class Operator {
public:
    typedef T value_type;
    typedef StateEntry entry_type;
    typedef StateUpdate<U> result_type;
    typedef StateUpdate<value_type> update_type;

    typedef const update_type& input_type;
    typedef std::optional<result_type> output_type;

    Operator() {}

    output_type process();

    void receive(input_type update);

protected:
};

};  // namespace storia
