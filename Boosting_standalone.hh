#pragma once

#if defined(BOOSTING) && defined(BOOSTING_STANDALONE)
#include "local_vector.hh"
#include "Boosting_common.hh"

class Boosting {
public:
  class Abort {};
  typedef void (*Callback)(void*, void*, void*);

  Boosting() : commitCallbacks(), abortCallbacks() {}

  void did_commit() {
    for (auto it = commitCallbacks.begin(); it != commitCallbacks.end(); ++it) {
      auto& cb = *it;
      cb.callback(cb.context1, cb.context2, cb.context3);
    }
    abortCallbacks.unsafe_clear();
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
    commitCallbacks.unsafe_clear();
  }

  void add_commit_callback(Callback func, void *c1, void *c2, void *c3) {
    commitCallbacks.emplace_back(func, c1, c2, c3);
    assert(commitCallbacks.size() == 1);
  }
  void add_abort_callback(Callback func, void *c1, void *c2, void *c3) {
    abortCallbacks.emplace_back(func, c1, c2, c3);
  }

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

extern Boosting __boostingtransactions[BOOSTING_MAX_THREADS];
#define BOOSTING_T() (__boostingtransactions[boosting_threadid])

#define TRANSACTION \
  do {              \
    while (1) {     \
      boosting_txStartHook(); \
      try {
#define RETRY(retry)                        \
        BOOSTING_T().did_commit();          \
        break;                              \
      } catch(Boosting::Abort e) {}         \
      BOOSTING_T().did_abort();             \
      if (!(retry))                         \
        throw Boosting::Abort();            \
    }                                       \
  } while(0)

#define DO_ABORT() (throw Boosting::Abort())
#define POST_COMMIT(callback, context1, context2, context3) BOOSTING_T().add_commit_callback((callback), (context1), (context2), (context3))
#define ON_ABORT(callback, context1, context2, context3) BOOSTING_T().add_abort_callback((callback), (context1), (context2), (context3))
#endif
