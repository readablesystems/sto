

class Reader {
public:
  virtual ~Reader() {}

  virtual bool check(void *context) = 0;
};

class Writer {
public:
  virtual ~Writer() {}

  virtual void lock() = 0;
  virtual void unlock() = 0;
  virtual uint64_t UID() = 0;
  virtual void install(void *context) = 0;
};
