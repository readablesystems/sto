// Used for working with commutative operations

#pragma once

#include "MVCCTypes.hh"

namespace commutators {

template <typename T>
class Commutator {
public:
    // Each type must define its own Commutator variant
    Commutator() = default;

    virtual void operate(T&) const {
        always_assert(false, "Should never operate on the default commutator.");
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

    virtual bool is_blind_write() const {
        return false;
    }

    virtual void operate(int64_t& v) const {
        v += delta;
    }

private:
    int64_t delta;
};

}
