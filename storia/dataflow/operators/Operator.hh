#pragma once

#include <optional>

#include "../State.hh"

namespace storia {

class Operator {
protected:
    Operator() {};

    template 
        <typename T, typename U=T, typename InputType=BlindWrite<T>,
    typename OutputType=BlindWrite<U>>
    class TypeSpec {
    public:
        typedef U update_type;
        typedef T value_type;

        typedef Entry entry_type;
        typedef InputType input_type;
        typedef OutputType output_type;

        typedef const input_type& arg_type;
        typedef std::optional<output_type> ret_type;
    };
};

};  // namespace storia
