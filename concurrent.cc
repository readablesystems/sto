#include <iostream>
#include <assert.h>
#include <random>
#include <climits>

#include "Array.hh"
#include "TransState.hh"

// size of array
#define N 100
#define NTHREADS 1

// only used for randomRWs test
#define NTRANS 1000000
#define NPERTRANS 10

#define GLOB_SEED 0

//#define DEBUG

#ifdef DEBUG
#define debug(...) printf(__VA_ARGS__)
#else
#define debug(...) /* */
#endif

Array<int, N> a;

using namespace std;


void *randomRWs(void *p) {
  int me = (intptr_t)p;
  
  std::uniform_int_distribution<> slotdist(0, N-1);
  std::uniform_int_distribution<> booldist(0,1);
  std::uniform_int_distribution<> seeddist(0, INT_MAX);
  std::mt19937 gen(me + GLOB_SEED);

  for (int i = 0; i < (NTRANS/NTHREADS); ++i) {
    // so that retries of this transaction do the same thing
    auto transseed = seeddist(gen);

    bool done = false;
    while (!done) {
      std::mt19937 transgen(transseed);

      TransState t;
      for (int j = 0; j < NPERTRANS; ++j) {
        int slot = slotdist(transgen);
        int r = booldist(transgen);
        if (r)
          a.transRead(t, slot);
        else
          a.transWrite(t, slot, j);
      }
      done = t.commit();
      if (!done) {
        debug("thread%d retrying\n", me);
      }
    }
  }
  return NULL;
}

// each thread's ops are deterministic but thread interleavings are not
// (could use only read-then-write operations (e.g. counters) to make verifiable random test)
void checkRandomRWs() {}

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

    if (a.transRead(t, 0) == 0 || me == NTHREADS-1) {
      for (int i = 1; i < N; ++i) {
        a.transWrite(t, i, me);
      }
    }

    // NTHREADS-1 always wins
    if (me == NTHREADS-1) {
      a.transWrite(t, 0, me);
    }

    done = t.commit();
    debug("thread %d %d\n", me, done);
  }

  return NULL;
}

void checkBlindWrites() {
  for (int i = 0; i < N; ++i) {
    debug("read %d\n", a.read(i));
    assert(a.read(i) == NTHREADS-1);
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
  Interfering,
  Random
};

Test tests[] = {
  {isolatedWrites, checkIsolatedWrites},
  {blindWrites, checkBlindWrites},
  {interferingRWs, checkInterferingRWs},
  {randomRWs, checkRandomRWs}
};



int main(int argc, char *argv[]) {
  if (argc != 2) {
    cout << "Usage: " << argv[0] << " test#" << endl;
    return 1;
  }
  auto test = atoi(argv[1]);
  startAndWait(NTHREADS, tests[test].threadfunc);
  tests[test].checkfunc();
}
