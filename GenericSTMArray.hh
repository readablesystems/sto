#pragma once
#include "GenericSTM.hh"

// wraps an array transactionally with our GenericSTM class
// not all that useful/fast, but mostly useful for testing

template <typename Value, unsigned Size = 100>
class GenericSTMArray {
public:
  GenericSTMArray() : stm_(), array_() {}

  Value transRead(Transaction& t, int i) {
    return stm_.transRead(t, &array_[i]);
  }
  void transWrite(Transaction& t, int i, const Value& v) {
    stm_.transWrite(t, &array_[i], v);
  }

  Value read(int i) {
    return array_[i];
  }
private:
  GenericSTM stm_;
  Value array_[Size];
};
