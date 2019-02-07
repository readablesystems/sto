// Used for working with commutative operations

#pragma once

#include "MVCCTypes.hh"

namespace commutators { 

template <typename T>
class Commutator {
public:
    // Each type must define its own Commutator variant
    Commutator() = default;

    T& operate(T& v) const {
        always_assert(false, "Should never operate on the default commutator.");
        return v;
    }
};

//////////////////////////////////////////////
//
// Commutator type for integral +/- operations
//
//////////////////////////////////////////////

template <>
class Commutator<int64_t> {
public:
    Commutator() = default;
    explicit Commutator(int64_t delta) : delta(delta) {}

    int64_t& operate(int64_t& v) const {
        v += delta;
        return v;
    }

private:
    int64_t delta;
};

}
