#pragma once

#include "../Logics.hh"
#include "Operator.hh"

namespace storia {

template <typename T, typename B=T, typename U=size_t>
class Count : Operator {
private:
    typedef TypeSpec<T, U> Spec;

public:
    typedef typename Spec::arg_type arg_type;
    typedef typename Spec::input_type input_type;
    typedef typename Spec::output_type output_type;
    typedef typename Spec::ret_type ret_type;
    typedef typename Spec::update_type update_type;

    typedef B base_type;
    typedef Predicate<input_type, base_type> predicate_type;

    Count() : predicate_(std::nullopt) {}
    Count(const predicate_type& predicate) : predicate_(predicate) {}
    Count(const typename predicate_type::comp_type& comparator)
        : predicate_(PredicateUtil::Make<input_type, base_type>(comparator)) {}

    ret_type consume(arg_type update) {
        update_type change = 0;
        if (predicate_) {
            change = predicate_->eval(update);
        } else {
            change = 1;
        }
        return output_type(change);
    }

private:
    std::optional<predicate_type> predicate_;
};

};  // namespace storia
