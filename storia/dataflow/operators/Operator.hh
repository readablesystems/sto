#pragma once

#include "../StateEntry.hh"
#include "../StateUpdate.hh"

namespace storia {

class Operator {
public:
    Operator() {}

    StateUpdate process() {
        auto su = StateUpdate();
        return su;
    }

    void receive(const StateUpdate& update) {
        (void)update;
    }

protected:
};

};  // namespace storia
