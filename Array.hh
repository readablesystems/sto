template <typename T, unsigned N>
class Array {
  typedef uint32_t Version;
  typedef unsigned Key;
  typedef T Value;

  struct internal_elem {
    std::mutex mutex;
    Version version;
    T val;
  };

  Array() {
    memset(data_, 0, N*sizeof(internal_elem));
  }

  T transRead(TransState& t, Key i) {
    internal_elem cur = data_[i];
    t.read(ArrayRead(this, i, cur.version));
    return cur.val;
  }

  void transWrite(TransState& t, Key i, Value v) {
    t.write(ArrayWrite(this, i, v));
  }

  internal_elem data_[N];

  class ArrayRead : Reader {
  public:
    bool check() {
      return a_->elem(i_).version == version_;
    }

  private:
    Array *a_;
    Key i_;
    Version version_;

    ArrayRead(Array *a, Key i, Version v) : a_(a), i_(i), version_(v) {}
  };

  class ArrayWrite : Writer {
  public:

    void lock() {
      a_->elem(i_).mutex.lock();
    }
    void unlock() {
      a_->elem(i_).mutex.unlock();
    }

    uint64_t UID() {
      return index_;
    }

    void install() {
      a_->elem(i_).val = val_;
      a_->elem(i_).version++;
      // unlock here or leave it to transaction mechanism?
    }


  private:
    Array *a_;
    Key i_;
    Value val_;

    ArrayWrite(Array *a, Key i, Value v) : a_(a), i_(i), val_(v) {}
  };

  internal_elem& elem(Key i) {
    return data_[i];
  }

  };

};
