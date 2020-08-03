#pragma once

#include "../Logics.hh"
#include "Operator.hh"

namespace storia {

template <typename TLeft, typename TRight, typename S /* synthesis type */>
class InnerJoin : Operator {
private:
    typedef TypeSpec<TLeft, S> SpecLeft;
    typedef TypeSpec<TRight, S> SpecRight;

public:
    typedef typename SpecLeft::arg_type arg_left_type;
    typedef typename SpecRight::arg_type arg_right_type;
    typedef typename SpecLeft::input_type input_left_type;
    typedef typename SpecRight::input_type input_right_type;
    typedef typename SpecLeft::output_type output_type;
    typedef typename SpecLeft::ret_type ret_type;
    typedef typename SpecLeft::update_type update_type;

    InnerJoin() {}

    ret_type consume_left(arg_left_type update) {
        return output_type(S::from_left(update.value()));
    }

    ret_type consume_right(arg_right_type update) {
        return output_type(S::from_right(update.value()));
    }

private:
};

};  // namespace storia
