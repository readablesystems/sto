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

// size of array (for hashtables or other non-array structures, this is the
// size of the key space)
#define ARRAY_SZ 1000000

#define USE_ARRAY 0
#define USE_HASHTABLE 1
#define USE_MASSTREE 2
#define USE_GENSTMARRAY 3
#define USE_LISTARRAY 4

// set this to USE_DATASTRUCTUREYOUWANT
#define DATA_STRUCTURE USE_ARRAY

// if true, each operation of a transaction will act on a different slot
#define ALL_UNIQUE_SLOTS 0

// use string values rather than ints
#define STRING_VALUES 0

// use unboxed strings in Masstree (only used if STRING_VALUES is set)
#define UNBOXED_STRINGS 0

// if 1 we just print the runtime, no diagnostic information or strings 
// (makes it easier to collect data using a script)
#define DATA_COLLECT 0

// If we have N keys, we make our hashtable have size N/HASHTABLE_LOAD_FACTOR
#define HASHTABLE_LOAD_FACTOR 2

// additional seed to randomness used in tests (otherwise each run of
// ./concurrent does the exact same operations)
#define GLOBAL_SEED 0

/* Track the array state during concurrent execution using atomic increments.
 * Turn off if you want the most accurate performance results.
 *
 * When on, a validation check of our random test is much stronger, because
 * we are checking our concurrent run not only with a single-threaded run
 * but also with a guaranteed correct implementation
 */
#define MAINTAIN_TRUE_ARRAY_STATE 1

// assert reading our writes works
#define TRY_READ_MY_WRITES 0

// whether to also do random deletes in the randomRWs test (for performance 
// only, we can't verify correctness from this)
#define RAND_DELETES 0

// track/report diagnostics on how good our randomness is
#define RANDOM_REPORT 0

// Masstree globals
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


struct ContainerBase_arraylike {
    static constexpr bool has_delete = false;
    template <typename T, typename K, typename V>
    static bool transInsert(T&, Transaction&, K, V) {
        return false;
    }
    template <typename T, typename K>
    static bool transDelete(T&, Transaction&, K) {
        return false;
    }
    template <typename T, typename K, typename V>
    static bool transUpdate(T&, Transaction&, K, V) {
        return false;
    }
    template <typename T>
    static void thread_init(T&) {
    }
};

struct ContainerBase_maplike {
    static constexpr bool has_delete = true;
    template <typename T, typename K, typename V>
    static bool transInsert(T& c, Transaction& txn, K key, V value) {
        return c.transInsert(txn, key, value);
    }
    template <typename T, typename K>
    static bool transDelete(T& c, Transaction& txn, K key) {
        return c.transDelete(txn, key);
    }
    template <typename T, typename K, typename V>
    static bool transUpdate(T& c, Transaction& txn, K key, V value) {
        return c.transUpdate(txn, key, value);
    }
    template <typename T>
    static void thread_init(T&) {
    }
};


template <int DS> struct Container {};

template <> struct Container<USE_ARRAY> : public ContainerBase_arraylike {
    typedef Array1<value_type, ARRAY_SZ> type;
};

template <> struct Container<USE_LISTARRAY> : public ContainerBase_maplike {
    typedef ListArray<value_type> type;
    template <typename K, typename V>
    static bool transUpdate(type&, Transaction&, K, V) {
        return false;
    }
};

template <> struct Container<USE_GENSTMARRAY> : public ContainerBase_arraylike {
    typedef GenericSTMArray<value_type, ARRAY_SZ> type;
};

template <> struct Container<USE_MASSTREE> : public ContainerBase_maplike {
#if STRING_VALUES && UNBOXED_STRINGS
    typedef MassTrans<value_type, versioned_str_struct> type;
#else
    typedef MassTrans<value_type> type;
#endif
    static void thread_init(type& c) {
        c.thread_init();
    }
};

template <> struct Container<USE_HASHTABLE> : public ContainerBase_maplike {
    typedef Hashtable<int, value_type, ARRAY_SZ/HASHTABLE_LOAD_FACTOR> type;
};

typedef Container<DATA_STRUCTURE> ContainerType;
typedef Container<DATA_STRUCTURE>::type ArrayType;
ArrayType* a;

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

template <typename T>
void prepopulate_func(T& a) {
  for (int i = 0; i < prepopulate; ++i) {
    Transaction t;
    a.transWrite(t, i, val(i+1));
    t.commit();
  }
}

template <typename T>
static void doRead(T& a, Transaction& t, int slot) {
  if (readMyWrites)
    a.transRead(t, slot);
#if 0
  else
    a.transRead_nocheck(t, slot);
#endif
}

template <typename T>
static void doWrite(T& a, Transaction& t, int slot, int& ctr) {
  if (blindRandomWrite) {
    if (readMyWrites) {
      a.transWrite(t, slot, val(ctr));
    } 
#if 0
else {
      a.transWrite_nocheck(t, slot, val(ctr));
    }
#endif
  } else {
    // increment current value (this lets us verify transaction correctness)
    if (readMyWrites) {
      auto v0 = a.transRead(t, slot);
      a.transWrite(t, slot, val(unval(v0)+1));
#if TRY_READ_MY_WRITES
          // read my own writes
          assert(a.transRead(t,slot) == v0+1);
          a.transWrite(t, slot, v0+2);
          // read my own second writes
          assert(a.transRead(t,slot) == v0+2);
#endif
    } else {
#if 0
      auto v0 = a.transRead_nocheck(t, slot);
      a.transWrite_nocheck(t, slot, val(unval(v0)+1));
#endif
    }
    ++ctr; // because we've done a read and a write
  }
}

template <typename T>
static inline void nreads(T& a, int n, Transaction& t, std::function<int(void)> slotgen) {
  for (int i = 0; i < n; ++i) {
    doRead(a, t, slotgen());
  }
}
template <typename T>
static inline void nwrites(T& a, int n, Transaction& t, std::function<int(void)> slotgen) {
  for (int i = 0; i < n; ++i) {
    doWrite(a, t, slotgen(), i);
  }
}

void *readThenWrite(void *p) {
  int me = (intptr_t)p;
  ContainerType::thread_init(*a);

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
      nreads(*a, OPS - OPS*write_percent, t, gen);
      nwrites(*a, OPS*write_percent, t, gen);

      done = t.commit();
      if (!done) {
        debug("thread%d retrying\n", me);
      }
    }
  }
  return NULL;
}


template <bool do_delete, typename C>
void random_rws(int me, typename C::type& a) {
  Transaction::threadid = me;
  C::thread_init(a);

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
        if (do_delete && r > (write_thresh+write_thresh/2)) {
          // TODO: this doesn't make that much sense if write_percent != 50%
          C::transDelete(a, t, slot);
        } else if (r > write_thresh) {
          doRead(a, t, slot);
        } else {
          doWrite(a, t, slot, j);

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
}


void *randomRWs_delete(void *p) {
  int me = (intptr_t)p;
  random_rws<ContainerType::has_delete, ContainerType>(me, *a);
  return NULL;
}

void *randomRWs_nodelete(void *p) {
  int me = (intptr_t)p;
  random_rws<false, ContainerType>(me, *a);
  return NULL;
}

void checkRandomRWs() {
  ArrayType *old = a;
  a = new ArrayType();
  ArrayType& check = *a;

  // rerun transactions one-by-one
#if MAINTAIN_TRUE_ARRAY_STATE
  maintain_true_array_state = !maintain_true_array_state;
#endif

  prepopulate_func(check);

  for (int i = 0; i < nthreads; ++i) {
    randomRWs_nodelete((void*)(intptr_t)i);
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


void *kingOfTheDelete(void *p) {
  int me = (intptr_t)p;
  Transaction::threadid = me;
  ContainerType::thread_init(*a);

  bool done = false;
  while (!done) {
    try {
      Transaction t;
      for (int i = 0; i < nthreads; ++i) {
        if (i != me) {
          ContainerType::transDelete(*a, t, i);
        } else {
          a->transWrite(t, i, val(i+1));
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
  ContainerType::thread_init(*a);

  // we never pick slot 0 so we can detect if table is populated
  std::uniform_int_distribution<long> slotdist(1, ARRAY_SZ-1);
  uint32_t delete_thresh = (uint32_t) (write_percent * Rand::max());

  int N = ntrans/nthreads;
  int OPS = opspertrans;

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
            ContainerType::transUpdate(*a, t, slot, val(slot + 1));
          } else if (!ContainerType::transInsert(*a, t, slot, val(slot+1))) {
            // we delete if the element is there and insert if it's not
            // this is essentially xor'ing the slot, so ordering won't matter
            ContainerType::transDelete(*a, t, slot);
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

  prepopulate_func(*a);

  for (int i = 0; i < nthreads; ++i) {
    xorDelete((void*)(intptr_t)i);
  }
  a = old;

  for (int i = 0; i < ARRAY_SZ; ++i) {
    assert(unval(a->read(i)) == unval(check.read(i)));
  }
}


void checkIsolatedWrites() {
  for (int i = 0; i < nthreads; ++i) {
    assert(unval(a->read(i)) == i+1);
  }
}

void *isolatedWrites(void *p) {
  int me = (intptr_t)p;
  Transaction::threadid = me;
  ContainerType::thread_init(*a);

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
  ContainerType::thread_init(*a);

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
  ContainerType::thread_init(*a);

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
  const char* name;
  void *(*threadfunc) (void *);
  void (*checkfunc) (void);
  bool prepopulate;
};

Test tests[] = {
    {"isolatedwrites", isolatedWrites, checkIsolatedWrites, true},
    {"blindwrites", blindWrites, checkBlindWrites, true},
    {"interferingwrites", interferingRWs, checkInterferingRWs, true},
    {"randomreadwrites (usually you want this)", randomRWs_nodelete, checkRandomRWs, true},
    {"readthenwrite", readThenWrite, NULL, true},
    {"kingofthedelete", kingOfTheDelete, checkKingOfTheDelete, true},
    {"xordelete", xorDelete, checkXorDelete, true},
    {"randomreadwrites-delete (uncheckable, use for benchmarking only)", randomRWs_delete, checkRandomRWs, true},
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
  printf("\nTests:\n");
  for (size_t ti = 0; ti != sizeof(tests)/sizeof(tests[0]); ++ti)
    printf(" %zu: %s\n", ti, tests[ti].name);
  printf("\n");
  exit(1);
}

int main(int argc, char *argv[]) {
  Clp_Parser *clp = Clp_NewParser(argc, argv, arraysize(options), options);

  int test = -1;

  int opt;
  while ((opt = Clp_Next(clp)) != Clp_Done) {
    switch (opt) {
    case Clp_NotOption:
        if (isdigit((unsigned char) clp->vstr[0]))
            test = atoi(clp->vstr);
        else {
            test = -1;
            for (size_t ti = 0; ti != sizeof(tests) / sizeof(tests[0]); ++ti)
                if (strcmp(tests[ti].name, clp->vstr) == 0) {
                    test = (int) ti;
                    break;
                }
        }
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

#if DATA_STRUCTURE == USE_MASSTREE
  Transaction::epoch_advance_callback = [] (unsigned) {
    // just advance blindly because of the way Masstree uses epochs
    globalepoch++;
  };
#endif

  a = new ArrayType();

  if (tests[test].prepopulate)
    prepopulate_func(*a);

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
  Transaction::print_stats();
  {
      using thd = threadinfo_t;
      thd tc = Transaction::tinfo_combined();
      printf("total_n: %llu, total_r: %llu, total_w: %llu, total_searched: %llu, total_aborts: %llu (%llu aborts at commit time)\n", tc.p(txp_total_n), tc.p(txp_total_r), tc.p(txp_total_w), tc.p(txp_total_searched), tc.p(txp_total_aborts), tc.p(txp_commit_time_aborts));
#if DATA_STRUCTURE == USE_MASSTREE
      printf("node aborts: %llu\n", (unsigned long long) node_aborts);
#endif
  }
#endif

  if (runCheck)
    tests[test].checkfunc();
}
