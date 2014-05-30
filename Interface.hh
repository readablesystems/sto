#pragma once

#include <stdint.h>

struct ReaderData;
struct WriterData;

class Reader {
public:
  virtual ~Reader() {}

  virtual bool check(ReaderData data) = 0;
  virtual bool is_locked(ReaderData data) = 0;
  virtual uint64_t UID(ReaderData data) const = 0;
};

class Writer {
public:
  virtual ~Writer() {}

  virtual void lock(WriterData data) = 0;
  virtual void unlock(WriterData data) = 0;
  virtual uint64_t UID(WriterData data) const = 0;
  virtual void install(WriterData data) = 0;
};
