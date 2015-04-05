#include <iostream>
#include <assert.h>
#include <random>
#include <climits>
#include <sys/time.h>
#include <sys/resource.h>

#include "Array.hh"
#include "Array1.hh"
#include "GenericSTMArray.hh"
#include "ListArray.hh"
#include "Hashtable.hh"
#include "Transaction.hh"
#include "clp.h"

#include "MassTrans.hh"

// size of array
#define ARRAY_SZ 10000

// only used for randomRWs test
#define GLOBAL_SEED 0
#define TRY_READ_MY_WRITES 0
#define MAINTAIN_TRUE_ARRAY_STATE 1

// if true, each operation of a transaction will act on a different slot
#define ALL_UNIQUE_SLOTS 0

#define DATA_COLLECT 0
#define HASHTABLE 0
#define HASHTABLE_LOAD_FACTOR 2
#define HASHTABLE_RAND_DELETES 0

#define MASSTREE 1

#define GENSTM_ARRAY 0
#define LIST_ARRAY 1

#define RANDOM_REPORT 0

#define STRING_VALUES 0
#define UNBOXED_STRINGS 0

kvepoch_t global_log_epoch = 0;
volatile uint64_t globalepoch = 1;     // global epoch, updated by main thread regularly
kvtimestamp_t initial_timestamp;
volatile bool recovering = false; // so don't add log entries, and free old value immediately

//#define DEBUG

#ifdef DEBUG
#define debug(...) printf(__VA_ARGS__)
#else
#define debug(...) /* */
#endif

#if STRING_VALUES
typedef std::string value_type;
#else
typedef int value_type;
#endif

#if !HASHTABLE
#if !MASSTREE
#if !GENSTM_ARRAY
#if !LIST_ARRAY
typedef Array1<value_type, ARRAY_SZ> ArrayType;
ArrayType *a;
#else
typedef ListArray<value_type> ArrayType;
ArrayType *a;
#endif
#else
typedef GenericSTMArray<value_type, ARRAY_SZ> ArrayType;
ArrayType *a;
#endif
#else
typedef MassTrans<value_type
#if STRING_VALUES && UNBOXED_STRINGS
, versioned_str_struct
#endif
> ArrayType;
ArrayType *a;
#endif
#else
// hashtable from int to int
typedef Hashtable<int, value_type, ARRAY_SZ/HASHTABLE_LOAD_FACTOR> ArrayType;
ArrayType *a;
#endif

bool readMyWrites = true;
bool runCheck = false;
int nthreads = 4;
int ntrans = 1000000;
int opspertrans = 10;
int prepopulate = ARRAY_SZ/10;
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
  Rand(result_type u, result_type v) : u(u|1), v(v|1) {}

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

inline value_type val(int v) {
#if STRING_VALUES
  char s[64];
  sprintf(s, "%d", v);
  return std::move(std::string(s));
#else
  return v;
#endif
}

inline int unval(const value_type& v) {
#if STRING_VALUES
  return atoi(v.c_str());
#else
  return v;
#endif
}

static void doRead(Transaction& t, int slot) {
  if (readMyWrites)
    a->transRead(t, slot);
#if 0
  else
    a->transRead_nocheck(t, slot);
#endif
}

static void doWrite(Transaction& t, int slot, int& ctr) {
  if (blindRandomWrite) {
    if (readMyWrites) {
      a->transWrite(t, slot, val(ctr));
    } 
#if 0
else {
      a->transWrite_nocheck(t, slot, val(ctr));
    }
#endif
  } else {
    // increment current value (this lets us verify transaction correctness)
    if (readMyWrites) {
      auto v0 = a->transRead(t, slot);
      a->transWrite(t, slot, val(unval(v0)+1));
#if TRY_READ_MY_WRITES
          // read my own writes
          assert(a->transRead(t,slot) == v0+1);
          a->transWrite(t, slot, v0+2);
          // read my own second writes
          assert(a->transRead(t,slot) == v0+2);
#endif
    } else {
#if 0
      auto v0 = a->transRead_nocheck(t, slot);
      a->transWrite_nocheck(t, slot, val(unval(v0)+1));
#endif
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
#if MASSTREE
  a->thread_init();
#endif
  
  std::uniform_int_distribution<long> slotdist(0, ARRAY_SZ-1);

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
#if MASSTREE
  a->thread_init();
#endif
  
  std::uniform_int_distribution<long> slotdist(0, ARRAY_SZ-1);
  uint32_t write_thresh = (uint32_t) (write_percent * Rand::max());

#if RANDOM_REPORT
  int *slot_spread = (int*)calloc(sizeof(*slot_spread) * ARRAY_SZ, 1);
  int firstretry = 0;
#endif

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
      uint32_t seed = transseed*3 + (uint32_t)me*N*7 + (uint32_t)GLOBAL_SEED*MAX_THREADS*N*11;
      auto seedlow = seed & 0xffff;
      auto seedhigh = seed >> 16;
      Rand transgen(seed, seedlow << 16 | seedhigh);
#if ALL_UNIQUE_SLOTS
      bool used[ARRAY_SZ] = {false};
#endif

      Transaction t;
      for (int j = 0; j < OPS; ++j) {
        int slot = slotdist(transgen);
#if ALL_UNIQUE_SLOTS
        while (used[slot]) slot = slotdist(transgen);
        used[slot]=true;
#endif
        auto r = transgen();
#if HASHTABLE_RAND_DELETES
        // TODO: this doesn't make that much sense if write_percent != 50%
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
#if RANDOM_REPORT
        if (i==0) firstretry++;
#endif
        debug("thread%d retrying\n", me);
      }
    }
#if MAINTAIN_TRUE_ARRAY_STATE
    if (maintain_true_array_state) {
        std::sort(slots_written, slots_written + nslots_written);
        auto itend = readMyWrites ? slots_written + nslots_written : std::unique(slots_written, slots_written + nslots_written);
        for (auto it = slots_written; it != itend; ++it) {
            __sync_add_and_fetch(&true_array_state[*it], 1);
#if RANDOM_REPORT
            slot_spread[*it]++;
#endif
        }
    }
#endif
  }

#if RANDOM_REPORT
  printf("firstretry: %d\n", firstretry);
  printf("slot distribution (%d trans, %d per trans): \n", N, OPS);
  uint64_t sum = 0;
  uint64_t total = 0;
  for (uint64_t i = 0; i < ARRAY_SZ; ++i) {
    total += slot_spread[i];
    sum += i*slot_spread[i];
  }
  int64_t avg = sum / total;
  printf("avg: %llu\n", avg);
  long double var = 0;
  for (int64_t i = 0; i < ARRAY_SZ; ++i) {
    var += slot_spread[i] * (i - avg) * (i - avg);
  }
  printf("stddev: %Lf\n", sqrt(var / total));

  int cur = 0;
  for (int i = 0; i < ARRAY_SZ; ++i) {
    cur += slot_spread[i];
    if (i % (ARRAY_SZ/100) == (ARRAY_SZ/100 - 1)) {
      printf("section %d has: %d elems\n", i / (ARRAY_SZ/100), cur);
      cur = 0;
    }
  }

  for (int i = 0; i < 1000; ++i) {
    if (slot_spread[i] > 0)
      printf("insertat: %d (%d)\n", i, slot_spread[i]);
  }
#endif

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

  for (int i = 0; i < prepopulate; ++i) {
    Transaction t;
    a->transWrite(t, i, val(0));
    t.commit();
  }

  for (int i = 0; i < nthreads; ++i) {
    randomRWs((void*)(intptr_t)i);
  }
#if MAINTAIN_TRUE_ARRAY_STATE
  maintain_true_array_state = !maintain_true_array_state;
#endif
  a = old;

  for (int i = 0; i < ARRAY_SZ; ++i) {
# if MAINTAIN_TRUE_ARRAY_STATE
    if (unval(a->read(i)) != true_array_state[i])
        fprintf(stderr, "index [%d]: parallel %d, atomic %d\n",
                i, unval(a->read(i)), true_array_state[i]);
# endif
    if (unval(a->read(i)) != unval(check.read(i)))
        fprintf(stderr, "index [%d]: parallel %d, sequential %d\n",
                i, unval(a->read(i)), unval(check.read(i)));
    assert(unval(check.read(i)) == unval(a->read(i)));
  }
}

#if HASHTABLE || MASSTREE

void *kingOfTheDelete(void *p) {
  int me = (intptr_t)p;
  Transaction::threadid = me;
#if MASSTREE
  a->thread_init();
#endif

  bool done = false;
  while (!done) {
    try {
      Transaction t;
      for (int i = 0; i < nthreads; ++i) {
        if (i != me) {
          a->transDelete(t, i);
        } else {
          a->transPut(t, i, val(i+1));
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
    if (unval(a->read(i))) {
      count++;
    }
  }
  assert(count==1);
}

void *xorDelete(void *p) {
  int me = (intptr_t)p;
  Transaction::threadid = me;
#if MASSTREE
  a->thread_init();
#endif

  // we never pick slot 0 so we can detect if table is populated
  std::uniform_int_distribution<long> slotdist(1, ARRAY_SZ-1);
  uint32_t delete_thresh = (uint32_t) (write_percent * Rand::max());

  int N = ntrans/nthreads;
  int OPS = opspertrans;

  if (me == 0) {
    // populate
    Transaction t;
    for (int i = 1; i < ARRAY_SZ; ++i) {
      a->transPut(t, i, val(i+1));
    }
    a->transPut(t, 0, val(1));
    assert(t.commit());
  } else {
    // wait for populated
    while (1) {
      try {
        Transaction t;
        if (unval(a->transRead(t, 0)) && t.commit()) {
          break;
        }
        t.commit();
      } catch (Transaction::Abort E) {}
    }
  }

  for (int i = 0; i < N; ++i) {
    auto transseed = i;
    bool done = false;
    while (!done) {
      Rand transgen(transseed + me + GLOBAL_SEED, transseed + me + GLOBAL_SEED);
      try {
        Transaction t;
        for (int j = 0; j < OPS; ++j) {
          int slot = slotdist(transgen);
          auto r = transgen();
          if (r > delete_thresh) {
            // can't do put/insert because that makes the results ordering dependent
            // (these updates don't actually affect the final state at all)
            a->transUpdate(t, slot, val(slot+1));
          } else if (!a->transInsert(t, slot, val(slot+1))) {
            // we delete if the element is there and insert if it's not
            // this is essentially xor'ing the slot, so ordering won't matter
            a->transDelete(t, slot);
          }
        }
        done = t.commit();
      } catch (Transaction::Abort E) {}
    }
  }
  
  return NULL;
}

void checkXorDelete() {
  ArrayType *old = a;
  ArrayType check;
  a = &check;
  
  for (int i = 0; i < nthreads; ++i) {
    xorDelete((void*)(intptr_t)i);
  }
  a = old;
  
  for (int i = 0; i < ARRAY_SZ; ++i) {
    assert(unval(a->read(i)) == unval(check.read(i)));
  }
}
#endif

void checkIsolatedWrites() {
  for (int i = 0; i < nthreads; ++i) {
    assert(unval(a->read(i)) == i+1);
  }
}

void *isolatedWrites(void *p) {
  int me = (intptr_t)p;
  Transaction::threadid = me;
#if MASSTREE
  a->thread_init();
#endif

  bool done = false;
  while (!done) {
    try{
    Transaction t;

    for (int i = 0; i < nthreads; ++i) {
      a->transRead(t, i);
    }
		
		a->transWrite(t, me, val(me+1));
    
    done = t.commit();
    } catch (Transaction::Abort E) {}
    debug("iter: %d %d\n", me, done);
  }
  return NULL;
}


void *blindWrites(void *p) {
  int me = (long long)p;
  Transaction::threadid = me;
#if MASSTREE
  a->thread_init();
#endif

  bool done = false;
  while (!done) {
    try{
    Transaction t;

    if (unval(a->transRead(t, 0)) == 0 || me == nthreads-1) {
      for (int i = 1; i < ARRAY_SZ; ++i) {
        a->transWrite(t, i, val(me));
      }
    }

    // nthreads-1 always wins
    if (me == nthreads-1) {
      a->transWrite(t, 0, val(me));
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
    assert(unval(a->read(i)) == nthreads-1);
  }
}

void *interferingRWs(void *p) {
  int me = (intptr_t)p;
  Transaction::threadid = me;
#if MASSTREE
  a->thread_init();
#endif

  bool done = false;
  while (!done) {
    try{
    Transaction t;

    for (int i = 0; i < ARRAY_SZ; ++i) {
      if ((i % nthreads) >= me) {
        auto cur = a->transRead(t, i);
        a->transWrite(t, i, val(unval(cur)+1));
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
    assert(unval(a->read(i)) == (i % nthreads)+1);
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
#if HASHTABLE || MASSTREE
  {kingOfTheDelete, checkKingOfTheDelete},
  {xorDelete, checkXorDelete},
#endif
};

enum {
  opt_test = 1, opt_nrmyw, opt_check, opt_nthreads, opt_ntrans, opt_opspertrans, opt_writepercent, opt_blindrandwrites, opt_prepopulate
};

static const Clp_Option options[] = {
  { "no-readmywrites", 'n', opt_nrmyw, 0, 0 },
  { "check", 'c', opt_check, 0, Clp_Negate },
  { "nthreads", 0, opt_nthreads, Clp_ValInt, Clp_Optional },
  { "ntrans", 0, opt_ntrans, Clp_ValInt, Clp_Optional },
  { "opspertrans", 0, opt_opspertrans, Clp_ValInt, Clp_Optional },
  { "writepercent", 0, opt_writepercent, Clp_ValDouble, Clp_Optional },
  { "blindrandwrites", 0, opt_blindrandwrites, 0, Clp_Negate },
  { "prepopulate", 0, opt_prepopulate, Clp_ValInt, Clp_Optional },
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
 --blindrandwrites, do blind random writes for random tests. makes checking impossible\n\
 --prepopulate=PREPOPULATE, prepopulate table with given number of items (default %d)\n",
         name, nthreads, ntrans, opspertrans, write_percent, prepopulate);
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
    case opt_prepopulate:
      prepopulate = clp->val.i;
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

#if MASSTREE
  Transaction::epoch_advance_callback = [] (unsigned) {
    // just advance blindly because of the way Masstree uses epochs
    globalepoch++;
  };
#endif

  ArrayType stack_arr;
  a = &stack_arr;

  for (int i = 0; i < prepopulate; ++i) {
    Transaction t;
    a->transWrite(t, i, val(0));
    t.commit();
  }

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
#define LLU(x) ((long long unsigned)x)
  printf("total_n: %llu, total_r: %llu, total_w: %llu, total_searched: %llu, total_aborts: %llu (%llu aborts at commit time)\n", LLU(total_n), LLU(total_r), LLU(total_w), LLU(total_searched), LLU(total_aborts), LLU(commit_time_aborts));
#if MASSTREE
  printf("node aborts: %llu\n", LLU(node_aborts));
#endif
#undef LLU
#endif

  if (runCheck)
    tests[test].checkfunc();
}
