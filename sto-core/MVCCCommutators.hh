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

}
