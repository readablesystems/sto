#include <iostream>
#include <assert.h>
#include <random>
#include <climits>
#include <sys/time.h>
#include <sys/resource.h>

#include "Array.hh"
#include "Hashtable.hh"
#include "Transaction.hh"
#include "clp.h"

// size of array
#define ARRAY_SZ 100000

// only used for randomRWs test
#define GLOBAL_SEED 0
#define TRY_READ_MY_WRITES 0
#define MAINTAIN_TRUE_ARRAY_STATE 1

#define DATA_COLLECT 0
#define HASHTABLE 1
#define HASHTABLE_LOAD_FACTOR 2
#define HASHTABLE_RAND_DELETES 0

//#define DEBUG

#ifdef DEBUG
#define debug(...) printf(__VA_ARGS__)
#else
#define debug(...) /* */
#endif

#if !HASHTABLE
typedef Array<int, ARRAY_SZ> ArrayType;
ArrayType *a;
#else
// hashtable from int to int
typedef Hashtable<int, int, ARRAY_SZ/HASHTABLE_LOAD_FACTOR> ArrayType;
ArrayType *a;
#endif

bool readMyWrites = true;
bool runCheck = false;
int nthreads = 4;
int ntrans = 1000000;
int opspertrans = 10;
double write_percent = 0.5;
bool blindRandomWrite = false;


using namespace std;

#if MAINTAIN_TRUE_ARRAY_STATE
bool maintain_true_array_state = true;
int true_array_state[ARRAY_SZ];
#endif

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


static void doRead(Transaction& t, int slot) {
  if (readMyWrites)
    a->transRead(t, slot);
  else
    a->transRead_nocheck(t, slot);
}

static void doWrite(Transaction& t, int slot, int& ctr) {
  if (blindRandomWrite) {
    if (readMyWrites) {
      a->transWrite(t, slot, ctr);
    } else {
      a->transWrite_nocheck(t, slot, ctr);
    }
  } else {
    // increment current value (this lets us verify transaction correctness)
    if (readMyWrites) {
      auto v0 = a->transRead(t, slot);
      a->transWrite(t, slot, v0+1);
#if TRY_READ_MY_WRITES
          // read my own writes
          assert(a->transRead(t,slot) == v0+1);
          a->transWrite(t, slot, v0+2);
          // read my own second writes
          assert(a->transRead(t,slot) == v0+2);
#endif
    } else {
      auto v0 = a->transRead_nocheck(t, slot);
      a->transWrite_nocheck(t, slot, v0+1);
    }
    ++ctr; // because we've done a read and a write
  }
}
  
static inline void nreads(int n, Transaction& t, std::function<int(void)> slotgen) {
  for (int i = 0; i < n; ++i) {
    doRead(t, slotgen());
  }
}
static inline void nwrites(int n, Transaction& t, std::function<int(void)> slotgen) {
  for (int i = 0; i < n; ++i) {
    doWrite(t, slotgen(), i);
  }
}

void *readThenWrite(void *p) {
  int me = (intptr_t)p;
  
  std::uniform_int_distribution<> slotdist(0, ARRAY_SZ-1);

  int N = ntrans/nthreads;
  int OPS = opspertrans;
  for (int i = 0; i < N; ++i) {
    // so that retries of this transaction do the same thing
    auto transseed = i;

    bool done = false;
    while (!done) {
      Rand transgen(transseed + me + GLOBAL_SEED, transseed + me + GLOBAL_SEED);

      auto gen = [&]() { return slotdist(transgen); };

      Transaction t;
      nreads(OPS - OPS*write_percent, t, gen);
      nwrites(OPS*write_percent, t, gen);

      done = t.commit();
      if (!done) {
        debug("thread%d retrying\n", me);
      }
    }
  }
  return NULL;
}

void *randomRWs(void *p) {
  int me = (intptr_t)p;
  Transaction::threadid = me;
  
  std::uniform_int_distribution<> slotdist(0, ARRAY_SZ-1);
  uint32_t write_thresh = (uint32_t) (write_percent * Rand::max());

  int N = ntrans/nthreads;
  int OPS = opspertrans;
  for (int i = 0; i < N; ++i) {
    // so that retries of this transaction do the same thing
    auto transseed = i;
#if MAINTAIN_TRUE_ARRAY_STATE
    int slots_written[OPS], nslots_written;
#endif

    bool done = false;
    while (!done) {
      try{
#if MAINTAIN_TRUE_ARRAY_STATE
      nslots_written = 0;
#endif
      Rand transgen(transseed + me + GLOBAL_SEED, transseed + me + GLOBAL_SEED);

      Transaction t;
      for (int j = 0; j < OPS; ++j) {
        int slot = slotdist(transgen);
        auto r = transgen();
#if HASHTABLE_RAND_DELETES
        if (r > (write_thresh+write_thresh/2)) {
          a->transDelete(t, slot);
        }else
#endif
        if (r > write_thresh) {
          doRead(t, slot);
        } else {
          doWrite(t, slot, j);

#if MAINTAIN_TRUE_ARRAY_STATE
          slots_written[nslots_written++] = slot;
#endif
        }
      }
      done = t.commit();
      } catch (Transaction::Abort E) {}
      if (!done) {
        debug("thread%d retrying\n", me);
      }
    }
#if MAINTAIN_TRUE_ARRAY_STATE
    if (maintain_true_array_state) {
        std::sort(slots_written, slots_written + nslots_written);
        auto itend = readMyWrites ? slots_written + nslots_written : std::unique(slots_written, slots_written + nslots_written);
        for (auto it = slots_written; it != itend; ++it)
            __sync_add_and_fetch(&true_array_state[*it], 1);
    }
#endif
  }
  return NULL;
}

void checkRandomRWs() {
  ArrayType *old = a;
  ArrayType check;

  // rerun transactions one-by-one
#if MAINTAIN_TRUE_ARRAY_STATE
  maintain_true_array_state = !maintain_true_array_state;
#endif
  a = &check;
  for (int i = 0; i < nthreads; ++i) {
    randomRWs((void*)(intptr_t)i);
  }
#if MAINTAIN_TRUE_ARRAY_STATE
  maintain_true_array_state = !maintain_true_array_state;
#endif
  a = old;

  for (int i = 0; i < ARRAY_SZ; ++i) {
# if MAINTAIN_TRUE_ARRAY_STATE
    if (a->read(i) != true_array_state[i])
        fprintf(stderr, "index [%d]: parallel %d, atomic %d\n",
                i, a->read(i), true_array_state[i]);
# endif
    if (a->read(i) != check.read(i))
        fprintf(stderr, "index [%d]: parallel %d, sequential %d\n",
                i, a->read(i), check.read(i));
    assert(check.read(i) == a->read(i));
  }
}

void *kingOfTheDelete(void *p) {
  assert(HASHTABLE);
  int me = (intptr_t)p;
  Transaction::threadid = me;

  bool done = false;
  while (!done) {
    try {
      Transaction t;
      for (int i = 0; i < nthreads; ++i) {
        if (i != me) {
          a->transDelete(t, i);
        } else {
          a->transPut(t, i, i+1);
        }
      }
      done = t.commit();
    } catch (Transaction::Abort E) {}
  }
  return NULL;
}

void checkKingOfTheDelete() {
  int count = 0;
  for (int i = 0; i < nthreads; ++i) {
    if (a->read(i)) {
      count++;
    }
  }
  assert(count==1);
}

void checkIsolatedWrites() {
  for (int i = 0; i < nthreads; ++i) {
    assert(a->read(i) == i+1);
  }
}

void *isolatedWrites(void *p) {
  int me = (intptr_t)p;

  bool done = false;
  while (!done) {
    try{
    Transaction t;

    for (int i = 0; i < nthreads; ++i) {
      a->transRead(t, i);
    }

    a->transWrite(t, me, me+1);
    
    done = t.commit();
    } catch (Transaction::Abort E) {}
    debug("iter: %d %d\n", me, done);
  }
  return NULL;
}


void *blindWrites(void *p) {
  int me = (long long)p;

  bool done = false;
  while (!done) {
    try{
    Transaction t;

    if (a->transRead(t, 0) == 0 || me == nthreads-1) {
      for (int i = 1; i < ARRAY_SZ; ++i) {
        a->transWrite(t, i, me);
      }
    }

    // nthreads-1 always wins
    if (me == nthreads-1) {
      a->transWrite(t, 0, me);
    }

    done = t.commit();
    } catch (Transaction::Abort E) {}
    debug("thread %d %d\n", me, done);
  }

  return NULL;
}

void checkBlindWrites() {
  for (int i = 0; i < ARRAY_SZ; ++i) {
    debug("read %d\n", a->read(i));
    assert(a->read(i) == nthreads-1);
  }
}

void *interferingRWs(void *p) {
  int me = (intptr_t)p;

  bool done = false;
  while (!done) {
    try{
    Transaction t;

    for (int i = 0; i < ARRAY_SZ; ++i) {
      if ((i % nthreads) >= me) {
        auto cur = a->transRead(t, i);
        a->transWrite(t, i, cur+1);
      }
    }

    done = t.commit();
    } catch (Transaction::Abort E) {}
    debug("thread %d %d\n", me, done);
  }
  return NULL;
}

void checkInterferingRWs() {
  for (int i = 0; i < ARRAY_SZ; ++i) {
    assert(a->read(i) == (i % nthreads)+1);
  }
}

void startAndWait(int n, void *(*start_routine) (void *)) {
  pthread_t tids[n];
  for (int i = 0; i < n; ++i) {
    pthread_create(&tids[i], NULL, start_routine, (void*)(intptr_t)i);
  }
  pthread_t advancer;
  pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
  pthread_detach(advancer);

  for (int i = 0; i < n; ++i) {
    pthread_join(tids[i], NULL);
  }
}

void print_time(struct timeval tv1, struct timeval tv2) {
  printf("%f\n", (tv2.tv_sec-tv1.tv_sec) + (tv2.tv_usec-tv1.tv_usec)/1000000.0);
}

struct Test {
  void *(*threadfunc) (void *);
  void (*checkfunc) (void);
};

Test tests[] = {
  {isolatedWrites, checkIsolatedWrites},
  {blindWrites, checkBlindWrites},
  {interferingRWs, checkInterferingRWs},
  {randomRWs, checkRandomRWs},
  {readThenWrite, NULL},
  {kingOfTheDelete, checkKingOfTheDelete},
};

enum {
  opt_test = 1, opt_nrmyw, opt_check, opt_nthreads, opt_ntrans, opt_opspertrans, opt_writepercent, opt_blindrandwrites,
};

static const Clp_Option options[] = {
  { "no-readmywrites", 'n', opt_nrmyw, 0, 0 },
  { "check", 'c', opt_check, 0, Clp_Negate },
  { "nthreads", 0, opt_nthreads, Clp_ValInt, Clp_Optional },
  { "ntrans", 0, opt_ntrans, Clp_ValInt, Clp_Optional },
  { "opspertrans", 0, opt_opspertrans, Clp_ValInt, Clp_Optional },
  { "writepercent", 0, opt_writepercent, Clp_ValDouble, Clp_Optional },
  { "blindrandwrites", 0, opt_blindrandwrites, 0, Clp_Negate },
};

static void help(const char *name) {
  printf("Usage: %s test-number [OPTIONS]\n\
Options:\n\
 -n, --no-readmywrites\n\
 -c, --check, run a check of the results afterwards\n\
 --nthreads=NTHREADS (default %d)\n\
 --ntrans=NTRANS, how many total transactions to run (they'll be split between threads) (default %d)\n\
 --opspertrans=OPSPERTRANS, how many operations to run per transaction (default %d)\n\
 --writepercent=WRITEPERCENT, probability with which to do writes versus reads (default %f)\n\
 --blindrandwrites, do blind random writes for random tests. makes checking impossible\n",
         name, nthreads, ntrans, opspertrans, write_percent);
  exit(1);
}

int main(int argc, char *argv[]) {
  Clp_Parser *clp = Clp_NewParser(argc, argv, arraysize(options), options);

  int test = -1;

  int opt;
  while ((opt = Clp_Next(clp)) != Clp_Done) {
    switch (opt) {
    case Clp_NotOption:
      test = atoi(clp->vstr);
      break;
    case opt_nrmyw:
      readMyWrites = false;
      break;
    case opt_check:
      runCheck = !clp->negated;
      break;
    case opt_nthreads:
      nthreads = clp->val.i;
      break;
    case opt_ntrans:
      ntrans = clp->val.i;
      break;
    case opt_opspertrans:
      opspertrans = clp->val.i;
      break;
    case opt_writepercent:
      write_percent = clp->val.d;
      break;
    case opt_blindrandwrites:
      blindRandomWrite = !clp->negated;
      break;
    default:
      help(argv[0]);
    }
  }
  Clp_DeleteParser(clp);

  if (test == -1) {
    help(argv[0]);
  }
  
  if (nthreads > MAX_THREADS) {
    printf("Asked for %d threads but MAX_THREADS is %d\n", nthreads, MAX_THREADS);
    exit(1);
  }

  ArrayType stack_arr;
  a = &stack_arr;
  struct timeval tv1,tv2;
  struct rusage ru1,ru2;
  gettimeofday(&tv1, NULL);
  getrusage(RUSAGE_SELF, &ru1);
  startAndWait(nthreads, tests[test].threadfunc);
  gettimeofday(&tv2, NULL);
  getrusage(RUSAGE_SELF, &ru2);
#if !DATA_COLLECT
  printf("real time: ");
#endif
  print_time(tv1,tv2);
#if !DATA_COLLECT
  printf("utime: ");
  print_time(ru1.ru_utime, ru2.ru_utime);
  printf("stime: ");
  print_time(ru1.ru_stime, ru2.ru_stime);
  printf("Ran test %d with: ARRAY_SZ: %d, readmywrites: %d, result check: %d, %d threads, %d transactions, %d ops per transaction, %f%% writes, blindrandwrites: %d\n\
 MAINTAIN_TRUE_ARRAY_STATE: %d, LOCAL_VECTOR: %d, SPIN_LOCK: %d, INIT_SET_SIZE: %d, GLOBAL_SEED: %d, TRY_READ_MY_WRITES: %d, PERF_LOGGING: %d\n",
         test, ARRAY_SZ, readMyWrites, runCheck, nthreads, ntrans, opspertrans, write_percent*100, blindRandomWrite,
         MAINTAIN_TRUE_ARRAY_STATE, LOCAL_VECTOR, SPIN_LOCK, INIT_SET_SIZE, GLOBAL_SEED, TRY_READ_MY_WRITES, PERF_LOGGING);
#endif

#if PERF_LOGGING
  printf("total_n: %llu, total_r: %llu, total_w: %llu, total_searched: %llu, total_aborts: %llu\n", total_n, total_r, total_w, total_searched, total_aborts);
#endif

  if (runCheck)
    tests[test].checkfunc();
}
