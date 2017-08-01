#pragma once

class TransItem;

enum class CCPolicy : int {none = 0, occ, lock, tictoc};

class ConflictPredictor {
public:
    virtual ~ConflictPredictor() {};
    virtual CCPolicy get_policy(TransItem& item) = 0;
};

template <unsigned N>
class TArrayAdaptiveConflictPredictor;
