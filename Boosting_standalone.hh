#pragma once

#if defined(BOOSTING) && defined(BOOSTING_STANDALONE)
#include "local_vector.hh"
#include "Boosting_common.hh"

class Boosting {
public:
  class Abort {};

  Boosting() : commitCallbacks(), abortCallbacks() {}

  void did_commit() {
    for (auto it = commitCallbacks.begin(); it != commitCallbacks.end(); ++it) {
      auto& cb = *it;
      cb.callback(cb.context1, cb.context2, cb.context3);
    }
    commitCallbacks.unsafe_clear();
  }
  void did_abort() {
    // iterate in reverse
    for (auto it = abortCallbacks.rbegin(); it != abortCallbacks.rend(); ++it) {
      auto& cb = *it;
      cb.callback(cb.context1, cb.context2, cb.context3);
    }
    // callbacks have trivial destructors
    abortCallbacks.unsafe_clear();
  }

  void add_commit_callback(Callback func, void *c1, void *c2, void *c3) {
    commitCallbacks.emplace_back(func, c1, c2, c3);
  }
  void add_abort_callback(Callback func, void *c1, void *c2, void *c3) {
    abortCallbacks.emplace_back(func, c1, c2, c3);
  }

  typedef void (*Callback)(void*, void*, void*);

private:
  struct _Callback {
    _Callback(Callback callback, void *context1, void *context2, void *context3) : callback(callback), context1(context1), context2(context2), context3(context3) {}
    Callback callback;
    void *context1;
    void *context2;
    void *context3;
  };

  // we only have 1 commit callback currently
  local_vector<_Callback, 1> commitCallbacks;
  local_vector<_Callback, 16> abortCallbacks;
};

// TODO: should maybe be an array indexed by threadid instead
extern __thread Boosting __boostingtransaction;

#define TRANSACTION \
  do { \
    while (1) { \
      try { 
#define RETRY \
        __boostingtransaction.did_commit(); \
        break; \
      } catch(Boosting::Abort e) {}           \
      __boostingtransaction.did_abort(); \
      if (!(retry)) \
        throw Boosting::Abort();                \
    } \
  } while(0)

#define DO_ABORT() (throw Boosting::Abort())
#define POST_COMMIT(callback, context1, context2, context3) __boostingtransaction.add_commit_callback((callback), (context1), (context2), (context3))
#define ON_ABORT(callback, context1, context2, context3) __boostingtransaction.add_abort_callback((callback), (context1), (context2), (context3))
#endif
