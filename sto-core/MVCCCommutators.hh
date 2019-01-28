// Used for working with commutative operations

#pragma once

#include "MVCCTypes.hh"

namespace commutators { 

template <typename T>
class MvCommutator {
public:
    // Each type must define its own MvCommutator variant
    MvCommutator() = delete;

    T& operate(T& v) {
        always_assert(false, "Should never operate on the default commutator.");
        return v;
    }
};

// MvCommutator type for integral +/- operations

template <>
class MvCommutator<int64_t> {
public:
    explicit MvCommutator(int64_t delta) : delta(delta) {}

    int64_t& operate(int64_t& v) {
        v += delta;
        return v;
    }

private:
    int64_t delta;
};

}
