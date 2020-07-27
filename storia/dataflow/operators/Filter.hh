#pragma once

#include <queue>

#include "../Logics.hh"
#include "Operator.hh"

namespace storia {

template <typename T, typename B=void>
class Filter : Operator {
private:
    typedef TypeSpec<T> Spec;

public:
    typedef typename Spec::arg_type arg_type;
    typedef typename Spec::input_type input_type;
    typedef typename Spec::output_type output_type;
    typedef typename Spec::ret_type ret_type;
    typedef typename Spec::update_type update_type;

    typedef B base_type;
    typedef Predicate<input_type, base_type> predicate_type;

    Filter(const predicate_type& predicate) : predicate_(predicate) {}
    Filter(const typename predicate_type::comp_type& comparator)
        : predicate_(PredicateUtil::Make<input_type, base_type>(comparator)) {}

    ret_type consume(arg_type update) {
        if (predicate_.eval(update)) {
            return update;
        }
        return std::nullopt;
    }

private:
    const predicate_type predicate_;
    std::queue<output_type> updates_;
};

};  // namespace storia
