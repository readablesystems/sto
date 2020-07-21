#pragma once

#include <queue>

#include "../Logics.hh"
#include "Operator.hh"

namespace storia {

template <typename T, typename B=void>
class Filter : Operator<T> {
private:
    typedef Operator<T> Base;

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

    Filter(const predicate_type& predicate) : predicate_(predicate) {}
    Filter(const typename predicate_type::comp_type& comparator)
        : predicate_(PredicateUtil::Make<input_type, base_type>(comparator)) {}

    ret_type consume(arg_type update) override {
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
