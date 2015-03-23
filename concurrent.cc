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
#include "Queue.hh"
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
#define USE_QUEUE 5

// set this to USE_DATASTRUCTUREYOUWANT
#define DATA_STRUCTURE USE_QUEUE

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
    static void init() {
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
    static void init() {
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
    static void init() {
        Transaction::epoch_advance_callback = [] (unsigned) {
            // just advance blindly because of the way Masstree uses epochs
            globalepoch++;
        };
    }
    static void thread_init(type& c) {
        c.thread_init();
    }
};

template <> struct Container<USE_HASHTABLE> : public ContainerBase_maplike {
    typedef Hashtable<int, value_type, ARRAY_SZ/HASHTABLE_LOAD_FACTOR> type;
};

#if DATA_STRUCTURE == 5
typedef Queue<value_type, ARRAY_SZ> QueueType;
QueueType* q;
QueueType* q2;
#endif

bool readMyWrites = true;
bool runCheck = false;
int nthreads = 4;
int ntrans = 1000000;
int opspertrans = 10;
int prepopulate = 
    ARRAY_SZ/10;
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

void prepopulate_func(int *array) {
  for (int i = 0; i < prepopulate; ++i) {
    array[i] = i+1;
  }
}

#if DATA_STRUCTURE == 5
// FUNCTIONS FOR QUEUE
void prepopulate_func() {
  for (int i = 0; i < prepopulate; ++i) {
    q->push(val(i));
  }
}

void empty_func() {
    while (!q->empty()) {
        q->pop();
    }
}
#endif

// FUNCTIONS FOR ARRAY/MAP-TYPE
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

struct Tester {
    Tester() {}
    virtual void initialize() = 0;
    virtual void run(int me) = 0;
    virtual bool check() { return false; }
};

template <int DS> struct DSTester : public Tester {
    DSTester() : a() {}
    void initialize();
    virtual bool prepopulate() { return true; }
    typedef Container<DS> container_type;
    typedef typename Container<DS>::type type;
  protected:
    type* a;
};

template <int DS> void DSTester<DS>::initialize() {
    a = new type;
    if (prepopulate()) {
        prepopulate_func(*a);
        prepopulate_func(true_array_state);
    }
    Container<DS>::init();
}


template <int DS> struct ReadThenWrite : public DSTester<DS> {
    ReadThenWrite() {}
    void run(int me);
};

template <int DS> void ReadThenWrite<DS>::run(int me) {
  Transaction::threadid = me;
  typename Container<DS>::type* a = this->a;
  Container<DS>::thread_init(*a);

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

      Transaction& t = Transaction::get_transaction();
      nreads(*a, OPS - OPS*write_percent, t, gen);
      nwrites(*a, OPS*write_percent, t, gen);

      done = t.try_commit();
      if (!done) {
        debug("thread%d retrying\n", me);
      }
    }
  }
}


template <int DS> struct RandomRWs_parent : public DSTester<DS> {
    RandomRWs_parent() {}
    template <bool do_delete> void do_run(int me);
};

template <int DS> template <bool do_delete>
void RandomRWs_parent<DS>::do_run(int me) {
  Transaction::threadid = me;
  typename Container<DS>::type* a = this->a;
  Container<DS>::thread_init(*a);

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

      Transaction& t = Transaction::get_transaction();
      for (int j = 0; j < OPS; ++j) {
        int slot = slotdist(transgen);
#if ALL_UNIQUE_SLOTS
        while (used[slot]) slot = slotdist(transgen);
        used[slot]=true;
#endif
        auto r = transgen();
        if (do_delete && r > (write_thresh+write_thresh/2)) {
          // TODO: this doesn't make that much sense if write_percent != 50%
          Container<DS>::transDelete(*a, t, slot);
        } else if (r > write_thresh) {
          doRead(*a, t, slot);
        } else {
          doWrite(*a, t, slot, j);

#if MAINTAIN_TRUE_ARRAY_STATE
          slots_written[nslots_written++] = slot;
#endif
        }
      }
      done = t.try_commit();
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


template <int DS, bool do_delete> struct RandomRWs : public RandomRWs_parent<DS> {
    RandomRWs() {}
    void run(int me);
    bool check();
};

template <int DS, bool do_delete> void RandomRWs<DS, do_delete>::run(int me) {
    this->template do_run<do_delete && Container<DS>::has_delete>(me);
}

template <int DS, bool do_delete> bool RandomRWs<DS, do_delete>::check() {
  if (do_delete && Container<DS>::has_delete)
      return false;

  typename Container<DS>::type* old = this->a;
  typename Container<DS>::type& ch = *(new typename Container<DS>::type);
  this->a = &ch;

  // rerun transactions one-by-one
#if MAINTAIN_TRUE_ARRAY_STATE
  maintain_true_array_state = !maintain_true_array_state;
#endif
  prepopulate_func(ch);

  for (int i = 0; i < nthreads; ++i) {
      this->template do_run<false>(i);
  }
#if MAINTAIN_TRUE_ARRAY_STATE
  maintain_true_array_state = !maintain_true_array_state;
#endif
  this->a = old;

  for (int i = 0; i < ARRAY_SZ; ++i) {
# if MAINTAIN_TRUE_ARRAY_STATE
    if (unval(old->read(i)) != true_array_state[i])
        fprintf(stderr, "index [%d]: parallel %d, atomic %d\n",
                i, unval(old->read(i)), true_array_state[i]);
# endif
    if (unval(old->read(i)) != unval(ch.read(i)))
        fprintf(stderr, "index [%d]: parallel %d, sequential %d\n",
                i, unval(old->read(i)), unval(ch.read(i)));
    assert(unval(ch.read(i)) == unval(old->read(i)));
  }
  return true;
}


template <int DS> struct KingDelete : public DSTester<DS> {
    KingDelete() {}
    void run(int me);
    bool check();
};

template <int DS> void KingDelete<DS>::run(int me) {
  Transaction::threadid = me;
  typename Container<DS>::type* a = this->a;
  Container<DS>::thread_init(*a);

  bool done = false;
  while (!done) {
    try {
      Transaction& t = Transaction::get_transaction();
      for (int i = 0; i < nthreads; ++i) {
        if (i != me) {
          Container<DS>::transDelete(*a, t, i);
        } else {
          a->transWrite(t, i, val(i+1));
        }
      }
      done = t.try_commit();
    } catch (Transaction::Abort E) {}
  }
}

template <int DS> bool KingDelete<DS>::check() {
  typename Container<DS>::type* a = this->a;
  int count = 0;
  for (int i = 0; i < nthreads; ++i) {
    if (unval(a->read(i))) {
      count++;
    }
  }
  assert(count==1);
  return true;
}


template <int DS> struct XorDelete : public DSTester<DS> {
    XorDelete() {}
    void run(int me);
    bool check();
};

template <int DS> void XorDelete<DS>::run(int me) {
  Transaction::threadid = me;
  typename Container<DS>::type* a = this->a;
  Container<DS>::thread_init(*a);

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
        Transaction& t = Transaction::get_transaction();
        for (int j = 0; j < OPS; ++j) {
          int slot = slotdist(transgen);
          auto r = transgen();
          if (r > delete_thresh) {
            // can't do put/insert because that makes the results ordering dependent
            // (these updates don't actually affect the final state at all)
            Container<DS>::transUpdate(*a, t, slot, val(slot + 1));
          } else if (!Container<DS>::transInsert(*a, t, slot, val(slot+1))) {
            // we delete if the element is there and insert if it's not
            // this is essentially xor'ing the slot, so ordering won't matter
            Container<DS>::transDelete(*a, t, slot);
          }
        }
        done = t.try_commit();
      } catch (Transaction::Abort E) {}
    }
  }
}

template <int DS> bool XorDelete<DS>::check() {
  typename Container<DS>::type* old = this->a;
  typename Container<DS>::type& ch = *(new typename Container<DS>::type);
  this->a = &ch;
  prepopulate_func(*this->a);

  for (int i = 0; i < nthreads; ++i) {
    run(i);
  }
  this->a = old;

  for (int i = 0; i < ARRAY_SZ; ++i) {
    assert(unval(old->read(i)) == unval(ch.read(i)));
  }
  return true;
}


template <int DS> struct IsolatedWrites : public DSTester<DS> {
    IsolatedWrites() {}
    void run(int me);
    bool check();
};

template <int DS> void IsolatedWrites<DS>::run(int me) {
  Transaction::threadid = me;
  typename Container<DS>::type* a = this->a;
  Container<DS>::thread_init(*a);

  bool done = false;
  while (!done) {
    try{
    Transaction& t = Transaction::get_transaction();

    for (int i = 0; i < nthreads; ++i) {
      a->transRead(t, i);
    }
    a->transWrite(t, me, val(me+1));

    done = t.try_commit();
    } catch (Transaction::Abort E) {}
    debug("iter: %d %d\n", me, done);
  }
}

template <int DS> bool IsolatedWrites<DS>::check() {
  for (int i = 0; i < nthreads; ++i) {
    assert(unval(this->a->read(i)) == i+1);
  }
  return true;
}


template <int DS> struct BlindWrites : public DSTester<DS> {
    BlindWrites() {}
    void run(int me);
    bool check();
};

template <int DS> void BlindWrites<DS>::run(int me) {
  Transaction::threadid = me;
  typename Container<DS>::type* a = this->a;
  Container<DS>::thread_init(*a);

  bool done = false;
  while (!done) {
    try{
    Transaction& t = Transaction::get_transaction();

    if (unval(a->transRead(t, 0)) == 0 || me == nthreads-1) {
      for (int i = 1; i < ARRAY_SZ; ++i) {
        a->transWrite(t, i, val(me));
      }
    }

    // nthreads-1 always wins
    if (me == nthreads-1) {
      a->transWrite(t, 0, val(me));
    }

    done = t.try_commit();
    } catch (Transaction::Abort E) {}
    debug("thread %d %d\n", me, done);
  }
}

template <int DS> bool BlindWrites<DS>::check() {
  for (int i = 0; i < ARRAY_SZ; ++i) {
    debug("read %d\n", this->a->read(i));
    assert(unval(this->a->read(i)) == nthreads-1);
  }
  return true;
}


template <int DS> struct InterferingRWs : public DSTester<DS> {
    InterferingRWs() {}
    void run(int me);
    bool check();
};

template <int DS> void InterferingRWs<DS>::run(int me) {
  Transaction::threadid = me;
  typename Container<DS>::type* a = this->a;
  Container<DS>::thread_init(*a);

  bool done = false;
  while (!done) {
    try{
    Transaction& t = Transaction::get_transaction();

    for (int i = 0; i < ARRAY_SZ; ++i) {
      if ((i % nthreads) >= me) {
        auto cur = a->transRead(t, i);
        a->transWrite(t, i, val(unval(cur)+1));
      }
    }

    done = t.try_commit();
    } catch (Transaction::Abort E) {}
    debug("thread %d %d\n", me, done);
  }
}

template <int DS> bool InterferingRWs<DS>::check() {
  for (int i = 0; i < ARRAY_SZ; ++i) {
    assert(unval(this->a->read(i)) == (i % nthreads)+1);
  }
  return true;
}

#if DATA_STRUCTURE == 5
void Qxordeleterun(int me) {
  Transaction::threadid = me;

  int N = ntrans/nthreads;
  int OPS = opspertrans;
 
  for (int i = 0; i < N; ++i) {
    while (1) {
      try {
        Transaction t;
        for (int j = 0; j < OPS; ++j) {
          value_type v = val(j);
          value_type u;
          if (!q->transFront(t, u)) {
            // we pop if the q is nonempty, push if it's empty
            q->transPush(t, v);
          }
          else
              q->transPop(t);
        }
        if (t.try_commit())
            break;
      } catch (Transaction::Abort E) {}
    }
  }
}

bool Qxordeletecheck() {
  QueueType* old = q;
  QueueType& ch =  *(new QueueType);
  q = &ch;

  empty_func();
  prepopulate_func();
  
  for (int i = 0; i < nthreads; ++i) {
    Qxordeleterun(i);
  }
  q = old;

  // check only while q or ch is nonempty (may have started xordelete at different indices into the q)
  while (!q->empty() && !ch.empty()) {
    assert (unval(q->pop()) == unval(ch.pop()));
  }
  return true;
}

void Qtransferrun(int me) {
  Transaction::threadid = me;

  int N = ntrans/nthreads;
  int OPS = opspertrans;

  for (int i = 0; i < N; ++i) {
    while (1) {
      try {
        Transaction t;
        for (int j = 0; j < OPS; ++j) {
          value_type v;
          // if q is nonempty, pop from q, push onto q2
          if (q->transFront(t, v)) {
            q->transPop(t);
            q2->transPush(t, v);
          }
        }
        if (t.try_commit())
            break;
      } catch (Transaction::Abort E) {}
    }
  }
}

bool Qtransfercheck() {
  // restore q to prepopulated state
  empty_func();
  prepopulate_func();

  // check if q2 and q are equivalent
  while (!q2->empty()) {
    assert (unval(q->pop()) == unval(q2->pop()));
  }
  return true;
}

struct QTester {
    int me;
};

void* xorrunfunc(void* x) {
    QTester* qt = (QTester*) x;
    Qxordeleterun(qt->me);
    return nullptr;
} 

void* transferrunfunc(void* x) {
    QTester* qt = (QTester*) x;
    Qtransferrun(qt->me);
    return nullptr;
} 

void qstartAndWait(int n, void*(*runfunc)(void*)) {
  pthread_t tids[n];
  QTester tester[n];

  for (int i = 0; i < n; ++i) {
      tester[i].me = i;
      pthread_create(&tids[i], NULL, runfunc, &tester[i]);
  }
  pthread_t advancer;
  pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
  pthread_detach(advancer);

  for (int i = 0; i < n; ++i) {
    pthread_join(tids[i], NULL);
  }
}
#endif

struct TesterPair {
    Tester* t;
    int me;
};

void* runfunc(void* x) {
    TesterPair* tp = (TesterPair*) x;
    tp->t->run(tp->me);
    return nullptr;
}

void startAndWait(int n, Tester* tester) {
  pthread_t tids[n];
  TesterPair testers[n];
  for (int i = 0; i < n; ++i) {
      testers[i].t = tester;
      testers[i].me = i;
      pthread_create(&tids[i], NULL, runfunc, &testers[i]);
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

#define MAKE_TESTER(name, desc, type, ...)            \
    {name, desc, 0, new type<0, ## __VA_ARGS__>},     \
    {name, desc, 1, new type<1, ## __VA_ARGS__>},     \
    {name, desc, 2, new type<2, ## __VA_ARGS__>},     \
    {name, desc, 3, new type<3, ## __VA_ARGS__>},     \
    {name, desc, 4, new type<4, ## __VA_ARGS__>}

struct Test {
    const char* name;
    const char* desc;
    int ds;
    Tester* tester;
} tests[] = {
    MAKE_TESTER("isolatedwrites", 0, IsolatedWrites),
    MAKE_TESTER("blindwrites", 0, BlindWrites),
    MAKE_TESTER("interferingwrites", 0, InterferingRWs),
    MAKE_TESTER("randomrw", "typically best choice", RandomRWs, false),
    MAKE_TESTER("readthenwrite", 0, ReadThenWrite),
    MAKE_TESTER("kingofthedelete", 0, KingDelete),
    MAKE_TESTER("xordelete", 0, XorDelete),
    MAKE_TESTER("randomrw-d", "uncheckable", RandomRWs, true)
};

struct {
    const char* name;
    int ds;
} ds_names[] = {
    {"array", USE_ARRAY},
    {"hashtable", USE_HASHTABLE},
    {"hash", USE_HASHTABLE},
    {"masstree", USE_MASSTREE},
    {"mass", USE_MASSTREE},
    {"genstm", USE_GENSTMARRAY},
    {"list", USE_LISTARRAY},
    {"queue", USE_QUEUE}
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
  size_t testidx = 0;
  for (size_t ti = 0; ti != sizeof(tests)/sizeof(tests[0]); ++ti)
      if (ti == 0 || strcmp(tests[ti].name, tests[ti-1].name) != 0) {
          if (tests[ti].desc)
              printf(" %zu: %-10s (%s)\n", testidx, tests[ti].name, tests[ti].desc);
          else
              printf(" %zu: %s\n", testidx, tests[ti].name);
          ++testidx;
      }
  printf("\nData structures:\n");
  for (size_t ti = 0; ti != sizeof(ds_names)/sizeof(ds_names[0]); ++ti)
      printf(" %s", ds_names[ti].name);
  printf("\n");
  exit(1);
}

int main(int argc, char *argv[]) {
  Clp_Parser *clp = Clp_NewParser(argc, argv, arraysize(options), options);

  int ds = DATA_STRUCTURE;
  const char* test_name = nullptr;

  int opt;
  while ((opt = Clp_Next(clp)) != Clp_Done) {
    switch (opt) {
    case Clp_NotOption:
        for (size_t ti = 0; ti != sizeof(ds_names) / sizeof(ds_names[0]); ++ti)
            if (strcmp(ds_names[ti].name, clp->vstr) == 0) {
                ds = ds_names[ti].ds;
                goto found_ds;
            }
        test_name = clp->vstr;
    found_ds: break;
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

#if DATA_STRUCTURE == 5 
    QueueType stack_q;
    QueueType stack_q2;
    q = &stack_q;
    q2 = &stack_q2;
    
    empty_func();
    prepopulate_func();    
    qstartAndWait(nthreads, xorrunfunc);
    assert(Qxordeletecheck());
    
    empty_func();
    prepopulate_func();
    qstartAndWait(nthreads, transferrunfunc);
    assert(Qtransfercheck());
#else 

  int testidx = 0;
  int test = -1;
  for (size_t ti = 0; ti != sizeof(tests) / sizeof(tests[0]); ++ti) {
      if (ti > 0 && strcmp(tests[ti].name, tests[ti-1].name) != 0)
          ++testidx;
      if (tests[ti].ds == ds
          && test_name
          && (isdigit((unsigned char) test_name[0])
              ? atoi(test_name) == testidx
              : strcmp(tests[ti].name, test_name) == 0)) {
          test = ti;
          break;
      }
  }

  if (test == -1) {
    help(argv[0]);
  }

  if (nthreads > MAX_THREADS) {
    printf("Asked for %d threads but MAX_THREADS is %d\n", nthreads, MAX_THREADS);
    exit(1);
  }

  Tester* tester = tests[test].tester;
  tester->initialize();

  struct timeval tv1,tv2;
  struct rusage ru1,ru2;
  gettimeofday(&tv1, NULL);
  getrusage(RUSAGE_SELF, &ru1);
  startAndWait(nthreads, tester);
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

  size_t dsi = 0;
  while (ds_names[dsi].ds != ds)
      ++dsi;
  printf("Ran test %s %s\n", tests[test].name, ds_names[dsi].name);
  printf("  ARRAY_SZ: %d, readmywrites: %d, result check: %d, %d threads, %d transactions, %d ops per transaction, %f%% writes, prepopulate: %d, blindrandwrites: %d\n \
 MAINTAIN_TRUE_ARRAY_STATE: %d, INIT_SET_SIZE: %d, GLOBAL_SEED: %d, PERF_LOGGING: %d\n",
         ARRAY_SZ, readMyWrites, runCheck, nthreads, ntrans, opspertrans, write_percent*100, prepopulate, blindRandomWrite,
         MAINTAIN_TRUE_ARRAY_STATE, INIT_SET_SIZE, GLOBAL_SEED, PERF_LOGGING);
#endif

#if PERF_LOGGING
  Transaction::print_stats();
  {
      using thd = threadinfo_t;
      thd tc = Transaction::tinfo_combined();
      printf("total_n: %llu, total_r: %llu, total_w: %llu, total_searched: %llu, total_aborts: %llu (%llu aborts at commit time)\n", tc.p(txp_total_n), tc.p(txp_total_r), tc.p(txp_total_w), tc.p(txp_total_searched), tc.p(txp_total_aborts), tc.p(txp_commit_time_aborts));
      if (ds == USE_MASSTREE)
          printf("node aborts: %llu\n", (unsigned long long) node_aborts);
  }
#endif

  if (runCheck)
      tester->check();
#endif
}

