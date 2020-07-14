#pragma once

#include "../Logics.hh"
#include "Operator.hh"

namespace storia {

template <typename T, typename B=T>
class Count : Operator<T, size_t> {
private:
    typedef Operator<T, size_t> Base;

public:
    using typename Base::arg_type;
    using typename Base::entry_type;
    using typename Base::input_type;
    using typename Base::output_type;
    using typename Base::ret_type;
    using typename Base::value_type;

    typedef B base_type;
    typedef Predicate<input_type, base_type> predicate_type;

    Count() : count_(0), predicate_(std::nullopt) {}
    Count(const predicate_type& predicate) : count_(0), predicate_(predicate) {}
    Count(const typename predicate_type::comp_type& comparator)
        : count_(0),
          predicate_(PredicateUtil::Make<input_type, base_type>(comparator)) {}

    void consume(arg_type update) {
        if (predicate_) {
            count_ += predicate_->eval(update);
        } else {
            count_++;
        }
    }

    ret_type produce() {
        return output_type(count_);
    }

private:
    size_t count_;
    std::optional<predicate_type> predicate_;
};

};  // namespace storia
