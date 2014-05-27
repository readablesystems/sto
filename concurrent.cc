#include <iostream>
#include <assert.h>
#include <random>

#include "Array.hh"
#include "TransState.hh"

// size of array
#define N 10
#define NTHREADS 2

// only used for randomRWs test
#define NTRANS 1000000
#define NPERTRANS 10

#define GLOB_SEED 0

#define BLIND_RANDOM_WRITE 0

//#define DEBUG

#ifdef DEBUG
#define debug(...) printf(__VA_ARGS__)
#else
#define debug(...) /* */
#endif

typedef Array<int, N> ArrayType;
ArrayType *a;

using namespace std;

struct Rand {
  typedef uint32_t result_type;

  result_type u, v;
  Rand(result_type u, result_type v) : u(u+1), v(v+1) {}

  inline result_type operator()() {
    v = 36969*(v & 65535) + (v >> 16);
    u = 18000*(u & 65535) + (u >> 16);
    return (v << 16) + u;
  }

  static constexpr result_type max() {
    return (uint32_t)-1;
  }

  static constexpr result_type min() {
    return 0;
  }
};

void *randomRWs(void *p) {
  int me = (intptr_t)p;
  
  std::uniform_int_distribution<> slotdist(0, N-1);
  std::uniform_int_distribution<> booldist(0,1);

  for (int i = 0; i < (NTRANS/NTHREADS); ++i) {
    // so that retries of this transaction do the same thing
    auto transseed = i;

    bool done = false;
    while (!done) {
      Rand transgen(transseed, me + GLOB_SEED);

      TransState t;
      for (int j = 0; j < NPERTRANS; ++j) {
        int slot = slotdist(transgen);
        int r = booldist(transgen);
        if (r) {
          a->transRead(t, slot);
        } else {
#if BLIND_RANDOM_WRITE
          a->transWrite(t, slot, j);
#else
          // increment current value (this lets us verify transaction correctness)
          auto v0 = a->transRead(t, slot);
          a->transWrite(t, slot, v0+1);
          ++j; // because we've done a read and a write
#endif
        }
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
void checkRandomRWs() {
#if !BLIND_RANDOM_WRITE
  ArrayType *old = a;
  ArrayType check;
  a = &check;
  // rerun transactions one-by-one
  for (int i = 0; i < NTHREADS; ++i) {
    randomRWs((void*)i);
  }

  for (int i = 0; i < N; ++i) {
    assert(old->read(i) == a->read(i));
  }
#endif
}

void checkIsolatedWrites() {
  for (int i = 0; i < NTHREADS; ++i) {
    assert(a->read(i) == i+1);
  }
}

void *isolatedWrites(void *p) {
  int me = (long long)p;

  bool done = false;
  while (!done) {
    TransState t;

    for (int i = 0; i < NTHREADS; ++i) {
      a->transRead(t, i);
    }

    a->transWrite(t, me, me+1);
    
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

    if (a->transRead(t, 0) == 0 || me == NTHREADS-1) {
      for (int i = 1; i < N; ++i) {
        a->transWrite(t, i, me);
      }
    }

    // NTHREADS-1 always wins
    if (me == NTHREADS-1) {
      a->transWrite(t, 0, me);
    }

    done = t.commit();
    debug("thread %d %d\n", me, done);
  }

  return NULL;
}

void checkBlindWrites() {
  for (int i = 0; i < N; ++i) {
    debug("read %d\n", a->read(i));
    assert(a->read(i) == NTHREADS-1);
  }
}

void *interferingRWs(void *p) {
  int me = (long long)p;

  bool done = false;
  while (!done) {
    TransState t;

    for (int i = 0; i < N; ++i) {
      if ((i % NTHREADS) >= me) {
        auto cur = a->transRead(t, i);
        a->transWrite(t, i, cur+1);
      }
    }

    done = t.commit();
    debug("thread %d %d\n", me, done);
  }
  return NULL;
}

void checkInterferingRWs() {
  for (int i = 0; i < N; ++i) {
    assert(a->read(i) == (i % NTHREADS)+1);
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
  ArrayType stack_arr;
  a = &stack_arr;
  auto test = atoi(argv[1]);
  startAndWait(NTHREADS, tests[test].threadfunc);
  tests[test].checkfunc();
}
