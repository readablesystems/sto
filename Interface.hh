#include <stdint.h>

#pragma once

class Reader {
public:
  virtual ~Reader() {}

  virtual bool check(void *data1, void *data2) = 0;
  virtual bool is_locked(void *data1, void *data2) = 0;
  virtual uint64_t UID(void *data1, void *data2) const = 0;
};

class Writer {
public:
  virtual ~Writer() {}

  virtual void lock(void *data1, void *data2) = 0;
  virtual void unlock(void *data1, void *data2) = 0;
  virtual uint64_t UID(void *data1, void *data2) const = 0;
  virtual void install(void *data1, void *data2) = 0;
};
