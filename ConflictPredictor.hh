#pragma once

#include "Interface.hh"
#include "TransItem.hh"

enum class CCPolicy : int {none = 0, occ, lock, tictoc};

class ConflictPredictor {
public:
    virtual CCPolicy get_policy(TransItem& item) = 0;
};

template <unsigned N>
class TArrayAdaptiveConflictPredictor;
