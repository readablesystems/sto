#include <stdint.h>

#pragma once

class Reader {
public:
  virtual ~Reader() {}

  virtual bool check() { return false; }
};

class Writer {
public:
  virtual ~Writer() {}

  virtual void lock() {}
  virtual void unlock() {}
  virtual uint64_t UID() { return 0; }
  virtual void install() {}
};
