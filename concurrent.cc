#include <iostream>
#include <assert.h>

#include "Array.hh"
#include "TransState.hh"

#define N 100
#define NTHREADS 10

//#define DEBUG

#ifdef DEBUG
#define debug(...) printf(__VA_ARGS__)
#else
#define debug(...) /* */
#endif

Array<int, N> a;

using namespace std;

void checkIsolatedWrites() {
  for (int i = 0; i < NTHREADS; ++i) {
    assert(a.read(i) == i+1);
  }
}

void *isolatedWrites(void *p) {
  int me = (long long)p;

  bool done = false;
  while (!done) {
    TransState t;

    for (int i = 0; i < NTHREADS; ++i) {
      a.transRead(t, i);
    }

    a.transWrite(t, me, me+1);
    
    done = t.commit();
    debug("iter: %d %d\n", me, done);
  }
  return NULL;
}


void *blindWrites(void *p) {
  int me = (long long)p;

  bool done = false;
  while (!done) {
    TransState t;

    if (a.transRead(t, 0) == 0 || me == 7) {
      for (int i = 1; i < N; ++i) {
        a.transWrite(t, i, me);
      }
    }

    // 7 always wins
    if (me == 7) {
      a.transWrite(t, 0, me);
    }

    done = t.commit();
    debug("thread %d %d\n", me, done);
  }

  return NULL;
}

void checkBlindWrites() {
  for (int i = 0; i < N; ++i) {
    assert(a.read(i) == 7);
  }
}

void *interferingRWs(void *p) {
  int me = (long long)p;

  bool done = false;
  while (!done) {
    TransState t;

    for (int i = 0; i < N; ++i) {
      if ((i % NTHREADS) >= me) {
        auto cur = a.transRead(t, i);
        a.transWrite(t, i, cur+1);
      }
    }

    done = t.commit();
    debug("thread %d %d\n", me, done);
  }
  return NULL;
}

void checkInterferingRWs() {
  for (int i = 0; i < N; ++i) {
    assert(a.read(i) == (i % NTHREADS)+1);
  }
}

void startAndWait(int n, void *(*start_routine) (void *)) {
  pthread_t tids[n];
  for (int i = 0; i < n; ++i) {
    pthread_create(&tids[i], NULL, start_routine, (void*)i);
  }

  for (int i = 0; i < n; ++i) {
    pthread_join(tids[i], NULL);
  }
}

struct Test {
  void *(*threadfunc) (void *);
  void (*checkfunc) (void);
};

enum {
  Isolated,
  Blind,
  Interfering
};

Test tests[] = {
  {isolatedWrites, checkIsolatedWrites},
  {blindWrites, checkBlindWrites},
  {interferingRWs, checkInterferingRWs}
};



int main() {
  auto test = Blind;
  startAndWait(NTHREADS, tests[test].threadfunc);
  tests[test].checkfunc();
}
