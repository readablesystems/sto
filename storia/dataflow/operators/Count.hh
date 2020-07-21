#pragma once

#include "../Logics.hh"
#include "Operator.hh"

namespace storia {

template <typename T, typename B=T, typename U=size_t>
class Count : Operator<T, U> {
private:
    typedef Operator<T, U> Base;

public:
    using typename Base::arg_type;
    using typename Base::entry_type;
    using typename Base::input_type;
    using typename Base::output_type;
    using typename Base::ret_type;
    using typename Base::update_type;
    using typename Base::value_type;

    typedef B base_type;
    typedef Predicate<input_type, base_type> predicate_type;

    Count() : predicate_(std::nullopt) {}
    Count(const predicate_type& predicate) : predicate_(predicate) {}
    Count(const typename predicate_type::comp_type& comparator)
        : predicate_(PredicateUtil::Make<input_type, base_type>(comparator)) {}

    ret_type consume(arg_type update) override {
        U change = 0;
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
