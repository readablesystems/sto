#pragma once

#include "../Logics.hh"
#include "Operator.hh"

namespace storia {

template <typename T, typename B=T>
class Count : Operator<T, size_t> {
private:
    typedef Operator<T, size_t> Base;

public:
    using typename Base::entry_type;
    using typename Base::input_type;
    using typename Base::output_type;
    using typename Base::result_type;
    using typename Base::update_type;
    using typename Base::value_type;

    typedef B base_type;
    typedef Predicate<update_type, base_type> predicate_type;

    Count() : count_(0), predicate_(std::nullopt) {}
    Count(const predicate_type& predicate) : count_(0), predicate_(predicate) {}

    output_type process() {
        return result_type::NewBlindWrite(count_);
    }

    void receive(input_type update) {
        if (predicate_) {
            count_ += predicate_->eval(update);
        } else {
            count_++;
        }
    }

private:
    size_t count_;
    std::optional<predicate_type> predicate_;
};

};  // namespace storia
