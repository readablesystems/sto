#pragma once
#include "GenericSTM.hh"

// wraps an array transactionally with our GenericSTM class
// not all that useful/fast, but mostly useful for testing

template <typename Value, unsigned Size = 100>
class GenericSTMArray {
public:
  GenericSTMArray() : stm_(), array_() {}

  Value transRead(int i) {
    return stm_.transRead(&array_[i]);
  }
  void transWrite(int i, const Value& v) {
    stm_.transWrite(&array_[i], v);
  }

  Value read(int i) {
    return array_[i];
  }
private:
  GenericSTM stm_;
  Value array_[Size];
};
