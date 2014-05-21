#include <stdint.h>

#pragma once

class Reader {
public:
  virtual ~Reader() {}

  virtual bool check() = 0;
};

class Writer {
public:
  virtual ~Writer() {}

  virtual void lock() = 0;
  virtual void unlock() = 0;
  virtual uint64_t UID() = 0;
  virtual void install() = 0;
};
