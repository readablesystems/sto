#pragma once

#include <stdint.h>
#include "TransItem.hh"

class Transaction;

class Shared {
public:
  virtual ~Shared() {}

  virtual bool check(TransItem& item, Transaction& t) = 0;

  virtual void lock(TransItem& item) = 0;
  virtual void unlock(TransItem& item) = 0;
  virtual void install(TransItem& item, uint32_t tid) = 0;

  // probably just needs to call destructor
  virtual void cleanup(TransItem& item, bool committed) {
      (void) item, (void) committed;
  }
};
