#pragma once

#include <optional>

#include "../State.hh"

namespace storia {

template 
    <typename T, typename U=T, typename InputType=state::BlindWriteUpdate<T>,
    typename OutputType=state::BlindWriteUpdate<U>>
class Operator {
public:
    typedef U update_type;
    typedef T value_type;

    typedef state::Entry entry_type;
    typedef InputType input_type;
    typedef OutputType output_type;

    typedef const input_type& arg_type;
    typedef std::optional<output_type> ret_type;

protected:
    Operator() {};

public:
    void consume(arg_type update);
    ret_type produce();
};

};  // namespace storia
