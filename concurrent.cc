#include <iostream>
#include <assert.h>
#include <random>
#include <climits>
#include <sys/time.h>
#include <sys/resource.h>

#include "TArray.hh"
#include "TGeneric.hh"
#include "Hashtable.hh"
#include "Queue.hh"
#include "Vector.hh"
#include "TVector.hh"
#include "Transaction.hh"
#include "IntStr.hh"
#include "clp.h"
#include "randgen.hh"

#include "MassTrans.hh"

// size of array (for hashtables or other non-array structures, this is the
// size of the key space)
#ifndef ARRAY_SZ
#define ARRAY_SZ 10000000
#endif

#define USE_ARRAY 0
#define USE_HASHTABLE 1
#define USE_MASSTREE 2
#define USE_TGENERICARRAY 4
#define USE_QUEUE 5
#define USE_VECTOR 6
#define USE_TVECTOR 7
#define USE_MASSTREE_STR 8
#define USE_HASHTABLE_STR 9
#define USE_ARRAY_NONOPAQUE 10

// set this to USE_DATASTRUCTUREYOUWANT
#define DATA_STRUCTURE USE_HASHTABLE

// if true, then all threads perform non-conflicting operations
#define NON_CONFLICTING 0

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
#define HASHTABLE_LOAD_FACTOR 1.1

// initial seed; if 0, set randomly from /dev/urandom
// this is the default value for `--seed`
#define GLOBAL_SEED 0

/* Track the array state during concurrent execution using atomic increments.
 * Turn off if you want the most accurate performance results.
 *
 * When on, a validation check of our random test is much stronger, because
 * we are checking our concurrent run not only with a single-threaded run
 * but also with a guaranteed correct implementation
 */
#define MAINTAIN_TRUE_ARRAY_STATE 0

// assert reading our writes works
#define TRY_READ_MY_WRITES 0

// whether to also do random deletes in the randomRWs test (for performance
// only, we can't verify correctness from this)
#define RAND_DELETES 0

// track/report diagnostics on how good our randomness is
#define RANDOM_REPORT 0

// Masstree globals
//kvepoch_t global_log_epoch = 0;
//kvtimestamp_t initial_timestamp;

#ifdef BOOSTING
#ifdef BOOSTING_STANDALONE
#include "Boosting_standalone.hh"
#else
#include "Boosting_sto.hh"
TransPessimisticLocking __pessimistLocking;
#endif
#include "Boosting_map.hh"
#endif

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

inline value_type val(int v) {
#if STRING_VALUES
  return std::to_string(v);
#else
  return v;
#endif
}

inline int unval(const value_type& v) {
#if STRING_VALUES
  return std::stoi(v);
#else
  return v;
#endif
}

inline std::string valtostr(value_type v) {
#if STRING_VALUES
  return v;
#else
  return std::to_string(v);
#endif
}

inline value_type strtoval(const std::string& s) {
#if STRING_VALUES
  return s;
#else
  return s.empty() ? 0 : std::stoi(s);
#endif
}

volatile mrcu_epoch_type active_epoch = 1;

unsigned initial_seeds[64];


template <int DS> struct Container {};

template <> struct Container<USE_ARRAY> {
    typedef TArray<value_type, ARRAY_SZ> type;
    typedef int index_type;
    static constexpr bool has_delete = false;
    value_type nontrans_get(index_type key) {
        return v_.nontrans_get(key);
    }
    value_type transGet(index_type key) {
        return v_.transGet(key);
    }
    void transPut(index_type key, value_type value) {
        v_.transPut(key, value);
    }
    static void init() {
    }
    static void thread_init(Container<USE_ARRAY>&) {
    }
private:
    type v_;
};

template <> struct Container<USE_ARRAY_NONOPAQUE> {
    typedef TArray<value_type, ARRAY_SZ, TNonopaqueWrapped> type;
    typedef int index_type;
    static constexpr bool has_delete = false;
    value_type nontrans_get(index_type key) {
        return v_.nontrans_get(key);
    }
    value_type transGet(index_type key) {
        return v_.transGet(key);
    }
    void transPut(index_type key, value_type value) {
        v_.transPut(key, value);
    }
    static void init() {
    }
    static void thread_init(Container<USE_ARRAY_NONOPAQUE>&) {
    }
private:
    type v_;
};

template <> struct Container<USE_VECTOR> {
    typedef Vector<value_type> type;
    typedef typename type::size_type index_type;
    static constexpr bool has_delete = false;
    Container() {
        v_.reserve(ARRAY_SZ);
        while (v_.nontrans_size() < ARRAY_SZ)
            v_.nontrans_push_back(value_type());
    }
    value_type nontrans_get(index_type key) {
        return v_.nontrans_get(key);
    }
    value_type transGet(index_type key) {
        return v_.transGet(key);
    }
    void transPut(index_type key, value_type value) {
        return v_.transUpdate(key, value);
    }
    static void init() {
    }
    static void thread_init(Container<USE_VECTOR>&) {
    }
private:
    type v_;
};

template <> struct Container<USE_TVECTOR> {
    typedef TVector<value_type> type;
    typedef typename type::size_type index_type;
    static constexpr bool has_delete = false;
    Container() {
        v_.nontrans_reserve(ARRAY_SZ);
        while (v_.nontrans_size() < ARRAY_SZ)
            v_.nontrans_push_back(value_type());
    }
    value_type nontrans_get(index_type key) {
        return v_.nontrans_get(key);
    }
    value_type transGet(index_type key) {
        return v_.transGet(key);
    }
    void transPut(index_type key, value_type value) {
        v_.transPut(key, value);
    }
    static void init() {
    }
    static void thread_init(Container<USE_TVECTOR>&) {
    }
private:
    type v_;
};

template <> struct Container<USE_TGENERICARRAY> {
    typedef int index_type;
    static constexpr bool has_delete = false;
    value_type nontrans_get(index_type key) {
        return a_[key];
    }
    value_type transGet(index_type key) {
        assert(key >= 0 && key < ARRAY_SZ);
        return stm_.read(&a_[key]);
    }
    void transPut(index_type key, value_type value) {
        assert(key >= 0 && key < ARRAY_SZ);
        stm_.write(&a_[key], value);
    }
    static void init() {
    }
    static void thread_init(Container<USE_TGENERICARRAY>&) {
    }
private:
    TGeneric stm_;
    value_type a_[ARRAY_SZ];
};

template <> struct Container<USE_MASSTREE> {
#if STRING_VALUES && UNBOXED_STRINGS
    typedef MassTrans<value_type, versioned_str_struct> type;
#else
    typedef MassTrans<value_type> type;
#endif
    typedef int index_type;
    static constexpr bool has_delete = true;
    value_type nontrans_get(index_type key) {
        TransactionGuard guard;
        value_type v;
        v_.transGet(IntStr(key), v);
        return v;
    }
    value_type transGet(index_type key) {
        value_type v;
        v_.transGet(IntStr(key), v);
        return v;
    }
    void transPut(index_type key, value_type value) {
        v_.transPut(IntStr(key).str(), value);
    }
    bool transDelete(index_type key) {
        return v_.transDelete(IntStr(key).str());
    }
    bool transInsert(index_type key, value_type value) {
        return v_.transInsert(IntStr(key).str(), value);
    }
    bool transUpdate(index_type key, value_type value) {
        return v_.transUpdate(IntStr(key).str(), value);
    }
    static void init() {
        Transaction::epoch_advance_callback = [] (unsigned) {
            // just advance blindly because of the way Masstree uses epochs
            globalepoch++;
        };
    }
    static void thread_init(Container<USE_MASSTREE>&) {
        type::thread_init();
    }
private:
    type v_;
};

template <> struct Container<USE_MASSTREE_STR> {
#if UNBOXED_STRINGS
    typedef MassTrans<std::string, versioned_str_struct> type;
#else
    typedef MassTrans<std::string> type;
#endif
    typedef int index_type;
    static constexpr bool has_delete = true;
    value_type nontrans_get(index_type key) {
        std::string v;
        {
            TransactionGuard guard;
            v_.transGet(IntStr(key), v);
        }
        return strtoval(v);
    }
    value_type transGet(index_type key) {
        std::string v;
        v_.transGet(IntStr(key), v);
        return strtoval(v);
    }
    void transPut(index_type key, value_type value) {
        v_.transPut(IntStr(key).str(), valtostr(value));
    }
    bool transDelete(index_type key) {
        return v_.transDelete(IntStr(key).str());
    }
    bool transInsert(index_type key, value_type value) {
        return v_.transInsert(IntStr(key).str(), valtostr(value));
    }
    bool transUpdate(index_type key, value_type value) {
        return v_.transUpdate(IntStr(key).str(), valtostr(value));
    }
    static void init() {
        Transaction::epoch_advance_callback = [] (unsigned) {
            // just advance blindly because of the way Masstree uses epochs
            globalepoch++;
        };
    }
    static void thread_init(Container<USE_MASSTREE_STR>&) {
        type::thread_init();
    }
private:
    type v_;
};

template <> struct Container<USE_HASHTABLE> {
#ifndef BOOSTING
    typedef Hashtable<int, value_type, true, static_cast<unsigned>(ARRAY_SZ/HASHTABLE_LOAD_FACTOR)> type;
#else
    typedef TransMap<int, value_type, static_cast<unsigned>(ARRAY_SZ/HASHTABLE_LOAD_FACTOR)> type;
#endif
    typedef int index_type;
    static constexpr bool has_delete = true;
    value_type nontrans_get(index_type key) {
        return v_.unsafe_get(key);
    }
    value_type transGet(index_type key) {
        value_type v = value_type();
        v_.transGet(key, v);
        return v;
    }
    void transPut(index_type key, value_type value) {
        v_.transPut(key, value);
    }
    bool transDelete(index_type key) {
        return v_.transDelete(key);
    }
    bool transInsert(index_type key, value_type value) {
        return v_.transInsert(key, value);
    }
    bool transUpdate(index_type key, value_type value) {
        return v_.transUpdate(key, value);
    }
    static void init() {
    }
    static void thread_init(Container<USE_HASHTABLE>&) {
    }
private:
    type v_;
};

template <> struct Container<USE_HASHTABLE_STR> {
    typedef Hashtable<int, std::string, false, static_cast<unsigned>(ARRAY_SZ/HASHTABLE_LOAD_FACTOR)> type;
    typedef int index_type;
    static constexpr bool has_delete = true;
    value_type nontrans_get(index_type key) {
        return strtoval(v_.unsafe_get(key));
    }
    value_type transGet(index_type key) {
        std::string v;
        v_.transGet(key, v);
        return strtoval(v);
    }
    void transPut(index_type key, value_type value) {
        v_.transPut(key, valtostr(value));
    }
    bool transDelete(index_type key) {
        return v_.transDelete(key);
    }
    bool transInsert(index_type key, value_type value) {
        return v_.transInsert(key, valtostr(value));
    }
    bool transUpdate(index_type key, value_type value) {
        return v_.transUpdate(key, valtostr(value));
    }
    static void init() {
    }
    static void thread_init(Container<USE_HASHTABLE_STR>&) {
    }
private:
    type v_;
};

#if DATA_STRUCTURE == USE_QUEUE
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

template <typename T>
void prepopulate_func(T& a) {
  for (int i = 0; i < prepopulate; ++i) {
      TRANSACTION {
          a.transPut(i, val(i+1));
      } RETRY(false);
  }
  std::cout << "Done prepopulating " << std::endl;
}

void prepopulate_func(int *array) {
  for (int i = 0; i < prepopulate; ++i) {
    array[i] = i+1;
  }
}

#if DATA_STRUCTURE == USE_QUEUE
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
static void doRead(T& a, int slot) {
  if (readMyWrites)
    a.transGet(slot);
}

template <typename T>
static void doWrite(T& a, int slot, int& ctr) {
  if (blindRandomWrite) {
    if (readMyWrites) {
      a.transPut(slot, val(ctr));
    }
  } else {
    // increment current value (this lets us verify transaction correctness)
    if (readMyWrites) {
      auto v0 = a.transGet(slot);
      a.transPut(slot, val(unval(v0)+1));
#if TRY_READ_MY_WRITES
          // read my own writes
          assert(a.transGet(slot) == v0+1);
          a.transPut(slot, v0+2);
          // read my own second writes
          assert(a.transGet(slot) == v0+2);
#endif
    }
    ++ctr; // because we've done a read and a write
  }
}

template <typename T>
static inline void nreads(T& a, int n, std::function<int(void)> slotgen) {
  for (int i = 0; i < n; ++i) {
    doRead(a, slotgen());
  }
}

template <typename T>
static inline void nwrites(T& a, int n, std::function<int(void)> slotgen) {
  for (int i = 0; i < n; ++i) {
    doWrite(a, slotgen(), i);
  }
}

template <typename T, bool Support = T::has_delete> struct DoDeleteHelper;
template <typename T> struct DoDeleteHelper<T, true> {
    static void run(T& a, typename T::index_type slot) {
        a.transDelete(slot);
    }
};
template <typename T> struct DoDeleteHelper<T, false> {
    static void run(T&, typename T::index_type) {
    }
};
template <typename T>
static void doDelete(T& a, int slot) {
    DoDeleteHelper<T>::run(a, slot);
}



struct Tester {
    Tester() {}
    virtual void initialize() = 0;
    virtual void run(int me) {
        (void) me;
        assert(0 && "Test not supported");
    }
    virtual bool check() { return false; }
};

template <int DS> struct DSTester : public Tester {
    DSTester() : a() {}
    void initialize();
    virtual bool prepopulate() { return true; }
    typedef Container<DS> container_type;
  protected:
    container_type* a;
};

template <int DS> void DSTester<DS>::initialize() {
    a = new container_type;
    if (prepopulate()) {
        prepopulate_func(*a);
#if MAINTAIN_TRUE_ARRAY_STATE
        prepopulate_func(true_array_state);
#endif
    }
    container_type::init();
}


template <int DS> struct ReadThenWrite : public DSTester<DS> {
    typedef typename DSTester<DS>::container_type container_type;
    ReadThenWrite() {}
    void run(int me);
};

template <int DS> void ReadThenWrite<DS>::run(int me) {
  TThread::set_id(me);
  container_type* a = this->a;
  container_type::thread_init(*a);

  std::uniform_int_distribution<long> slotdist(0, ARRAY_SZ-1);
  Rand transgen(initial_seeds[2*me], initial_seeds[2*me + 1]);

  int N = ntrans/nthreads;
  int OPS = opspertrans;
  for (int i = 0; i < N; ++i) {
    // so that retries of this transaction do the same thing
    Rand transgen_snap = transgen;
    TRANSACTION {
      transgen = transgen_snap;
      auto gen = [&]() { return slotdist(transgen); };
      nreads(*a, OPS - OPS*write_percent, gen);
      nwrites(*a, OPS*write_percent, gen);
    } RETRY(true);
  }
}


template <int DS> struct RandomRWs_parent : public DSTester<DS> {
    typedef typename DSTester<DS>::container_type container_type;
    RandomRWs_parent() {}
    template <bool do_delete> void do_run(int me);
};

template <int DS> template <bool do_delete>
void RandomRWs_parent<DS>::do_run(int me) {
  TThread::set_id(me);
  Sto::update_threadid();
#ifdef BOOSTING_STANDALONE
  boosting_threadid = me;
#endif
  container_type* a = this->a;
  container_type::thread_init(*a);

#if NON_CONFLICTING
  long range = ARRAY_SZ/nthreads;
  std::uniform_int_distribution<long> slotdist(me*range, (me + 1) * range - 1);
#else
  std::uniform_int_distribution<long> slotdist(0, ARRAY_SZ-1);
#endif

  uint32_t write_thresh = (uint32_t) (write_percent * Rand::max());
  Rand transgen(initial_seeds[2*me], initial_seeds[2*me + 1]);

#if RANDOM_REPORT
  int *slot_spread = (int*)calloc(sizeof(*slot_spread) * ARRAY_SZ, 1);
  int firstretry = 0;
#endif

  int N = ntrans/nthreads;
  int OPS = opspertrans;
  for (int i = 0; i < N; ++i) {
    // so that retries of this transaction do the same thing
    Rand transgen_snap = transgen;
#if MAINTAIN_TRUE_ARRAY_STATE
    int slots_written[OPS], nslots_written;
#endif

    TRANSACTION {
#if MAINTAIN_TRUE_ARRAY_STATE
      nslots_written = 0;
#endif
      transgen = transgen_snap;
#if ALL_UNIQUE_SLOTS
      bool used[ARRAY_SZ] = {false};
#endif

      for (int j = 0; j < OPS; ++j) {
        int slot = slotdist(transgen);
#if ALL_UNIQUE_SLOTS
        while (used[slot]) slot = slotdist(transgen);
        used[slot]=true;
#endif
        auto r = transgen();
        if (do_delete && r > (write_thresh+write_thresh/2)) {
          // TODO: this doesn't make that much sense if write_percent != 50%
          doDelete(*a, slot);
        } else if (r > write_thresh) {
          doRead(*a, slot);
        } else {
          doWrite(*a, slot, j);

#if MAINTAIN_TRUE_ARRAY_STATE
          slots_written[nslots_written++] = slot;
#endif
        }
      }
    } RETRY(true);
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
    typedef typename DSTester<DS>::container_type container_type;
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

  container_type* old = this->a;
  container_type* ch = new container_type;;
  this->a = ch;

  // rerun transactions one-by-one
#if MAINTAIN_TRUE_ARRAY_STATE
  maintain_true_array_state = !maintain_true_array_state;
#endif
  prepopulate_func(*ch);

  for (int i = 0; i < nthreads; ++i) {
      this->template do_run<false>(i);
  }
#if MAINTAIN_TRUE_ARRAY_STATE
  maintain_true_array_state = !maintain_true_array_state;
#endif
  this->a = old;

  for (int i = 0; i < ARRAY_SZ; ++i) {
# if MAINTAIN_TRUE_ARRAY_STATE
    if (old->nontrans_get(i) != true_array_state[i])
        fprintf(stderr, "index [%d]: parallel %d, atomic %d\n",
                i, old->nontrans_get(i), true_array_state[i]);
# endif
    if (old->nontrans_get(i) != ch->nontrans_get(i))
        fprintf(stderr, "index [%d]: parallel %d, sequential %d\n",
                i, old->nontrans_get(i), ch->nontrans_get(i));
    assert(old->nontrans_get(i) == ch->nontrans_get(i));
  }
  delete ch;
  return true;
}


template <int DS, bool Ok = Container<DS>::has_delete> struct KingDelete;
template <int DS> struct KingDelete<DS, false> : public DSTester<DS> {};
template <int DS> struct KingDelete<DS, true> : public DSTester<DS> {
    typedef typename DSTester<DS>::container_type container_type;
    KingDelete() {}
    void run(int me);
    bool check();
};

template <int DS> void KingDelete<DS, true>::run(int me) {
  TThread::set_id(me);
  container_type* a = this->a;
  Container<DS>::thread_init(*a);

  TRANSACTION {
    for (int i = 0; i < nthreads; ++i) {
        if (i != me) {
          a->transDelete(i);
        } else {
          a->transPut(i, val(i+1));
        }
      }
  } RETRY(true);
}

template <int DS> bool KingDelete<DS, true>::check() {
  container_type* a = this->a;
  int count = 0;
  for (int i = 0; i < nthreads; ++i) {
    if (a->nontrans_get(i)) {
      count++;
    }
  }
  assert(count==1);
  return true;
}


template <int DS, bool Ok = Container<DS>::has_delete> struct XorDelete;
template <int DS> struct XorDelete<DS, false> : public DSTester<DS> {};
template <int DS> struct XorDelete<DS, true> : public DSTester<DS> {
    typedef typename DSTester<DS>::container_type container_type;
    XorDelete() {}
    void run(int me);
    bool check();
};

template <int DS> void XorDelete<DS, true>::run(int me) {
  TThread::set_id(me);
  Sto::update_threadid();
#ifdef BOOSTING_STANDALONE
  boosting_threadid = me;
#endif
  container_type* a = this->a;
  container_type::thread_init(*a);

  // we never pick slot 0 so we can detect if table is populated
  std::uniform_int_distribution<long> slotdist(1, ARRAY_SZ-1);
  uint32_t delete_thresh = (uint32_t) (write_percent * Rand::max());
  Rand transgen(initial_seeds[2*me], initial_seeds[2*me + 1]);

  int N = ntrans/nthreads;
  int OPS = opspertrans;

  for (int i = 0; i < N; ++i) {
    Rand transgen_snap = transgen;
    TRANSACTION {
        transgen = transgen_snap;
        for (int j = 0; j < OPS; ++j) {
          int slot = slotdist(transgen);
          auto r = transgen();
          if (r > delete_thresh) {
            // can't do put/insert because that makes the results ordering dependent
            // (these updates don't actually affect the final state at all)
            a->transUpdate(slot, val(slot + 1));
          } else if (!a->transInsert(slot, val(slot+1))) {
            // we delete if the element is there and insert if it's not
            // this is essentially xor'ing the slot, so ordering won't matter
            a->transDelete(slot);
          }
        }
    } RETRY(true);
  }
}

template <int DS> bool XorDelete<DS, true>::check() {
  container_type* old = this->a;
  container_type* ch = new container_type;
  this->a = ch;
  prepopulate_func(*this->a);

  for (int i = 0; i < nthreads; ++i) {
    run(i);
  }
  this->a = old;

  for (int i = 0; i < ARRAY_SZ; ++i) {
    assert(old->nontrans_get(i) == ch->nontrans_get(i));
  }
  delete ch;
  return true;
}


template <int DS> struct IsolatedWrites : public DSTester<DS> {
    typedef typename DSTester<DS>::container_type container_type;
    IsolatedWrites() {}
    void run(int me);
    bool check();
};

template <int DS> void IsolatedWrites<DS>::run(int me) {
  TThread::set_id(me);
  container_type* a = this->a;
  container_type::thread_init(*a);

  TRANSACTION {
    for (int i = 0; i < nthreads; ++i) {
      a->transGet(i);
    }
    a->transPut(me, val(me+1));
  } RETRY(true);
}

template <int DS> bool IsolatedWrites<DS>::check() {
  for (int i = 0; i < nthreads; ++i) {
    assert(this->a->nontrans_get(i) == i+1);
  }
  return true;
}


template <int DS> struct BlindWrites : public DSTester<DS> {
    typedef typename DSTester<DS>::container_type container_type;
    BlindWrites() {}
    void run(int me);
    bool check();
};

template <int DS> void BlindWrites<DS>::run(int me) {
  TThread::set_id(me);
  container_type* a = this->a;
  container_type::thread_init(*a);

  TRANSACTION {
    if (unval(a->transGet(0)) == 0 || me == nthreads-1) {
      for (int i = 1; i < ARRAY_SZ; ++i) {
        a->transPut(i, val(me));
      }
    }

    // nthreads-1 always wins
    if (me == nthreads-1) {
      a->transPut(0, val(me));
    }
  } RETRY(true);
}

template <int DS> bool BlindWrites<DS>::check() {
  for (int i = 0; i < ARRAY_SZ; ++i) {
    debug("read %d\n", this->a->nontrans_get(i));
    assert(this->a->nontrans_get(i) == nthreads-1);
  }
  return true;
}


template <int DS> struct InterferingRWs : public DSTester<DS> {
    typedef typename DSTester<DS>::container_type container_type;
    InterferingRWs() {}
    void run(int me);
    bool check();
};

template <int DS> void InterferingRWs<DS>::run(int me) {
  TThread::set_id(me);
  container_type* a = this->a;
  container_type::thread_init(*a);

  TRANSACTION {
    for (int i = 0; i < ARRAY_SZ; ++i) {
      if ((i % nthreads) >= me) {
        auto cur = a->transGet(i);
        a->transPut(i, val(unval(cur)+1));
      }
    }
  } RETRY(true);
}

template <int DS> bool InterferingRWs<DS>::check() {
  for (int i = 0; i < ARRAY_SZ; ++i) {
    assert(this->a->nontrans_get(i) == (i % nthreads)+1);
  }
  return true;
}

#if DATA_STRUCTURE == USE_QUEUE
void Qxordeleterun(int me) {
  TThread::set_id(me);
  Sto::update_threadid();

  int N = ntrans/nthreads;
  int OPS = opspertrans;
 
  for (int i = 0; i < N; ++i) {
    TRANSACTION {
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
    } RETRY(true);
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
  TThread::set_id(me);

  int N = ntrans/nthreads;
  int OPS = opspertrans;

  for (int i = 0; i < N; ++i) {
    TRANSACTION {
        for (int j = 0; j < OPS; ++j) {
          value_type v;
          // if q is nonempty, pop from q, push onto q2
          if (q->transFront(t, v)) {
            q->transPop(t);
            q2->transPush(t, v);
          }
        }
    } RETRY(true);
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
    {name, desc, 4, new type<4, ## __VA_ARGS__>},     \
    {name, desc, 6, new type<6, ## __VA_ARGS__>},     \
    {name, desc, 7, new type<7, ## __VA_ARGS__>},     \
    {name, desc, 8, new type<8, ## __VA_ARGS__>},     \
    {name, desc, 9, new type<9, ## __VA_ARGS__>},     \
    {name, desc, 10, new type<10, ## __VA_ARGS__>}

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
    {"array-nonopaque", USE_ARRAY_NONOPAQUE},
    {"hashtable", USE_HASHTABLE},
    {"hash", USE_HASHTABLE},
    {"hash-str", USE_HASHTABLE_STR},
    {"masstree", USE_MASSTREE},
    {"mass", USE_MASSTREE},
    {"masstree-str", USE_MASSTREE_STR},
    {"tgeneric", USE_TGENERICARRAY},
    {"queue", USE_QUEUE},
    {"vector", USE_VECTOR},
    {"tvector", USE_TVECTOR}
};

enum {
    opt_test = 1, opt_nrmyw, opt_check, opt_nthreads, opt_ntrans, opt_opspertrans, opt_writepercent, opt_blindrandwrites, opt_prepopulate, opt_seed
};

static const Clp_Option options[] = {
  { "no-readmywrites", 'n', opt_nrmyw, 0, 0 },
  { "check", 'c', opt_check, 0, Clp_Negate },
  { "nthreads", 'j', opt_nthreads, Clp_ValInt, Clp_Optional },
  { "ntrans", 0, opt_ntrans, Clp_ValInt, Clp_Optional },
  { "opspertrans", 0, opt_opspertrans, Clp_ValInt, Clp_Optional },
  { "writepercent", 0, opt_writepercent, Clp_ValDouble, Clp_Optional },
  { "blindrandwrites", 0, opt_blindrandwrites, 0, Clp_Negate },
  { "prepopulate", 0, opt_prepopulate, Clp_ValInt, Clp_Optional },
  { "seed", 's', opt_seed, Clp_ValUnsigned, 0 }
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
 --prepopulate=PREPOPULATE, prepopulate table with given number of items (default %d)\n\
 --seed=SEED\n",
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
  unsigned seed = GLOBAL_SEED;

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
    case opt_seed:
        seed = clp->val.u;
        break;
    default:
      help(argv[0]);
    }
  }
  Clp_DeleteParser(clp);

    if (seed)
        srandom(seed);
    else
        srandomdev();
    for (unsigned i = 0; i < arraysize(initial_seeds); ++i)
        initial_seeds[i] = random();

#if DATA_STRUCTURE == USE_QUEUE
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
 MAINTAIN_TRUE_ARRAY_STATE: %d, INIT_SET_SIZE: %d, GLOBAL_SEED: %d, STO_PROFILE_COUNTERS: %d\n",
         ARRAY_SZ, readMyWrites, runCheck, nthreads, ntrans, opspertrans, write_percent*100, prepopulate, blindRandomWrite,
         MAINTAIN_TRUE_ARRAY_STATE, Transaction::tset_initial_capacity, seed, STO_PROFILE_COUNTERS);
  printf("  STO_SORT_WRITESET: %d\n", STO_SORT_WRITESET);
#endif

#if STO_PROFILE_COUNTERS
  Transaction::print_stats();
  if (txp_count >= txp_total_aborts) {
      txp_counters tc = Transaction::txp_counters_combined();
      const char* sep = "";
      if (txp_count > txp_total_w) {
          printf("%stotal_n: %llu, total_r: %llu, total_w: %llu", sep, tc.p(txp_total_n), tc.p(txp_total_r), tc.p(txp_total_w));
          sep = ", ";
      }
      if (txp_count > txp_total_searched) {
          printf("%stotal_searched: %llu", sep, tc.p(txp_total_searched));
          sep = ", ";
      }
      if (txp_count > txp_total_aborts) {
          printf("%stotal_aborts: %llu (%llu aborts at commit time)\n", sep, tc.p(txp_total_aborts), tc.p(txp_commit_time_aborts));
          sep = ", ";
      }
      if (*sep)
          printf("\n");
  }
#endif

  if (runCheck) {
    if (tester->check())
      printf("Check succeeded\n");
    else
      printf("CHECK FAILED\n");
  }
#endif
}

