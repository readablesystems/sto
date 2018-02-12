#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <set>
#include <random>
#include <thread>
#include <climits>
#include <cassert>
#include <cstdlib>
#include <sys/time.h>
#include <sys/resource.h>
#include <fstream>

#include "PlatformFeatures.hh"

#include "TFlexArray.hh"
//#include "TGeneric.hh"
//#include "Hashtable.hh"
//#include "Queue.hh"
//#include "Vector.hh"
//#include "TVector.hh"
//#include "Transaction.hh"
#include "IntStr.hh"
#include "clp.h"
#include "randgen.hh"
//#include "SwissTGeneric.hh"
#include "sampling.hh"
#include "SystemProfiler.hh"

//#include "MassTrans.hh"

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
//#define USE_VECTOR 6
#define USE_TVECTOR 7
#define USE_MASSTREE_STR 8
#define USE_HASHTABLE_STR 9
#define USE_ARRAY_NONOPAQUE 10
#define USE_ARRAY_ADAPTIVE 11
#define USE_SWISSARRAY 12
//#define USE_SWISSGENERICARRAY 13
#define USE_ARRAY_TICTOC 14

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

//volatile mrcu_epoch_type active_epoch = 1;

unsigned initial_seeds[64];


template <int DS> struct Container {};


template <> struct Container<USE_ARRAY> {
    typedef TFlexArray<value_type, ARRAY_SZ, TOpaqueWrapped> type;
    typedef int index_type;
    static constexpr bool has_delete = false;
    value_type nontrans_get(index_type key) {
        return v_.nontrans_get(key);
    }
    void nontrans_put(index_type key, const value_type& val) {
        v_.nontrans_put(key, val);
    }
    bool transGet(index_type key, value_type& ret) {
        return v_.transGet(key, ret);
    }
    bool transPut(index_type key, value_type value) {
        return v_.transPut(key, value);
    }
    static void init() {
    } 
    void init_ns() {
    }
    void finalize() {
    }
    static void thread_init(Container<USE_ARRAY>&) {
    }
private:
    type v_;
};

template <> struct Container<USE_SWISSARRAY> {
    typedef TSwissArray<value_type, ARRAY_SZ> type;
    typedef int index_type;
    static constexpr bool has_delete = false;
    value_type nontrans_get(index_type key) {
        return v_.nontrans_get(key);
    }
    void nontrans_put(index_type key, const value_type& val) {
        v_.nontrans_put(key, val);
    }
    bool transGet(index_type key, value_type& ret) {
        return v_.transGet(key, ret);
    }
    bool transPut(index_type key, value_type value) {
        return v_.transPut(key, value);
    }
    static void init() {
    }
    void init_ns() {
    }
    void finalize() {
    } 
    static void thread_init(Container<USE_SWISSARRAY>&) {
    }
private:
    type v_;
};

template <> struct Container<USE_ARRAY_NONOPAQUE> {
    typedef TOCCArray<value_type, ARRAY_SZ> type;
    typedef int index_type;
    static constexpr bool has_delete = false;
    value_type nontrans_get(index_type key) {
        return v_.nontrans_get(key);
    }
    void nontrans_put(index_type key, const value_type& val) {
        v_.nontrans_put(key, val);
    }
    bool transGet(index_type key, value_type& ret) {
        return v_.transGet(key, ret);
    }
    bool transPut(index_type key, value_type value) {
        return v_.transPut(key, value);
    }
    static void init() {
    }
    void init_ns() {
    }
    void finalize() {
    }
    static void thread_init(Container<USE_ARRAY_NONOPAQUE>&) {
    }
private:
    type v_;
};

template <> struct Container<USE_ARRAY_ADAPTIVE> {
    typedef TAdaptiveArray<value_type, ARRAY_SZ> type;
    typedef int index_type;
    static constexpr bool has_delete = false;
    value_type nontrans_get(index_type key) {
        return v_.nontrans_get(key);
    }
    void nontrans_put(index_type key, const value_type& val) {
        v_.nontrans_put(key, val);
    }
    bool transGet(index_type key, value_type& ret) {
        return v_.transGet(key, ret);
    }
    bool transPut(index_type key, value_type value) {
        return v_.transPut(key, value);
    }
    static void init() {}
    void init_ns() {}
    void finalize() {}
    static void thread_init(Container<USE_ARRAY_ADAPTIVE>&) {}
private:
    type v_;
};

template <> struct Container<USE_ARRAY_TICTOC> {
    typedef TicTocArray<value_type, ARRAY_SZ> type;
    typedef int index_type;
    static constexpr bool has_delete = false;
    value_type nontrans_get(index_type key) {
        return v_.nontrans_get(key);
    }
    void nontrans_put(index_type key, const value_type& val) {
        v_.nontrans_put(key, val);
    }
    bool transGet(index_type key, value_type& ret) {
        return v_.transGet(key, ret);
    }
    bool transPut(index_type key, value_type value) {
        return v_.transPut(key, value);
    }
    static void init() {}
    void init_ns() {}
    void finalize() {}
    static void thread_init(Container<USE_ARRAY_TICTOC>&) {}
private:
    type v_;
};

/*
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
    void init_ns() {
    }
    void finalize() {
    }
    static void thread_init(Container<USE_VECTOR>&) {
    }
private:
    type v_;
};
*/

#if 0
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
    void init_ns() {
    }
    void finalize() {
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
    void init_ns() {
    }
    void finalize() {
    }
    static void thread_init(Container<USE_TGENERICARRAY>&) {
    }
private:
    TGeneric stm_;
    value_type a_[ARRAY_SZ];
};*/

template <> struct Container<USE_SWISSGENERICARRAY> {
    typedef int index_type;
    static constexpr bool has_delete = false;
    value_type nontrans_get(index_type key) {
        return a_[key];
    }
    bool transGet(index_type key, value_type& ret) {
        return stm_.read(&a_[key], ret);
    }
    bool transPut(index_type key, value_type value) {
        return stm_.write(&a_[key], value);
    }
    static void init() {
    }
    void init_ns() {
        std::cout << "Container<USE_SWISSGENERIC>::init starts!" << std::endl;
        //stm_.init_table_counts();
    }
    void finalize() {
        std::cout << "Container<USE_SWISSGENERIC>::finalize starts!" << std::endl;
        //stm_.print_table_counts();
    }
    static void thread_init(Container<USE_SWISSGENERICARRAY>&) {
    }
private:
    //SwissTGeneric stm_;
    SwissTNonopaqueGeneric stm_;
    value_type a_[ARRAY_SZ] __attribute__ ((aligned (sizeof(value_type))));
};


/*template <> struct Container<USE_MASSTREE> {
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
    void init_ns() {
    }
    void finalize() {
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
    void init_ns() {
    }
    void finalize() {
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
    void init_ns() {
    }
    void finalize() {
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
    void init_ns() {
    } 
    void finalize() {
    }
    static void thread_init(Container<USE_HASHTABLE_STR>&) {
    }
private:
type v_;
};*/

#endif

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
int opspertrans_ro = -1;
int prepopulate = ARRAY_SZ;//ARRAY_SZ/10;
double readonly_percent = 0.0;
double write_percent = 0.5;
bool blindRandomWrite = true;
double zipf_skew = 1.0;
bool profile = false;
bool dump_trace = false;
bool stop = false; // global stop signal


using namespace std;

#if MAINTAIN_TRUE_ARRAY_STATE
bool maintain_true_array_state = true;
int true_array_state[ARRAY_SZ];
#endif

template <typename T>
void prepopulate_func(T& a) {
  for (int i = 0; i < prepopulate; ++i) {
      a.nontrans_put(i, val(i+1));
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
static bool doRead(T& a, int slot, value_type& ret) {
    return a.transGet(slot, ret);
}

template <typename T>
static bool doWrite(T& a, int slot, int& ctr) {
    return a.transPut(slot, val(ctr));
//#if 0
//else {
    // increment current value (this lets us verify transaction correctness)
    // if (readMyWrites) {
    //  ++ctr;
    //  value_type ret;
    //  if (!a.transGet(slot, ret))
    //      return false;
    //  return a.transPut(slot, val(unval(ret)+1));
#if 0
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
#endif
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
virtual void finalize() = 0;
virtual void run(int me, uint64_t start_tsc) {
(void) me; (void)start_tsc;
assert(0 && "Test not supported");
}
virtual bool check() { return false; }
virtual void report() {
#if DEBUG_SKEW
std::cout << "skew reporting not available for this test"
    << std::endl;
#endif
}
};

template <int DS> struct DSTester : public Tester {
DSTester() : a() {}
void initialize();
void finalize() {
  a->finalize();
}
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
    a->init_ns();
}

// New test: Random R/W with zipf distribution to simulate skewed contention
enum class OpType : int {read, write, inc};

struct RWOperation {
    RWOperation() : type(OpType::read), key(), value() {}
    RWOperation(OpType t, sampling::index_t k, value_type v = value_type())
        : type(t), key(k), value(v) {}

    OpType type;
    sampling::index_t key;
    value_type value;
};

inline std::ostream& operator<<(std::ostream& os, const RWOperation& op) {
    os << "[";
    if (op.type == OpType::read) {
        os << "r,k=" << op.key;
    } else if (op.type == OpType::write) {
        os << "w,k=" << op.key << ",v=" << op.value;
    } else {
        assert(op.type == OpType::inc);
        os << "inc,k=" << op.key;
    }
    os << "]";
    return os;
}

inline std::ostream& operator <<(std::ostream& os, const std::vector<RWOperation>& query) {
    os << "{";
    for (auto& op : query)
        os << op << ",";
    os << "}" << std::endl;
    return os;
}

typedef struct skew_account_struct {
    unsigned long time_to_stop;  // in clock ticks
    unsigned long ntxns_at_stop;
    unsigned long time_to_quota; // in clock ticks

    skew_account_struct() {
        time_to_stop = 0;
        ntxns_at_stop = 0;
        time_to_quota = 0;
    }
} skew_account_t;

template <int DS>
struct HotspotRW : public DSTester<DS> {
    typedef typename DSTester<DS>::container_type container_type;
    typedef std::vector<RWOperation> query_type;
    typedef std::vector<query_type> workload_type;
    HotspotRW() {
        for (int i = 0; i < MAX_THREADS; ++i) {
            progress[i].reserve(100000);
            optimistic_rate[i].reserve(100000);
        }
    }
    void run(int me, uint64_t start_tsc) override;
    bool prepopulate() override;
    void report() override;

    std::vector<workload_type> workloads;
    std::vector<uint64_t> progress[MAX_THREADS];
    std::vector<double> optimistic_rate[MAX_THREADS];
    virtual void per_thread_workload_init(int thread_id);

#if DEBUG_SKEW
    std::vector<skew_account_t> skew_account;
#endif
};

inline void dump_thread_trace(int thread_id, const std::vector<std::vector<RWOperation>>& workload) {
    std::stringstream fn;
    fn << "thread_dump_" << thread_id << ".txt";
    std::fstream fs(fn.str(), std::ios::out | std::ios::trunc);
    if (!fs.is_open()) {
        std::cerr << "Cannot open file " << fn.str() << std::endl;
        assert(false);
    }
    for (auto& query : workload) {
        fs << query;
    }
    fs.flush();
    fs.close();
}

template <int DS>
void HotspotRW<DS>::per_thread_workload_init(int thread_id) {
    sampling::StoUniformDistribution ud(thread_id, 0, std::numeric_limits<uint32_t>::max());

    auto& thread_workload = workloads[thread_id];

    int trans_per_thread = ntrans / nthreads;
    uint64_t readonly_ceil = (uint64_t)(readonly_percent * std::numeric_limits<uint32_t>::max());
    int write_thresh = (int)(write_percent * opspertrans);

    for (int i = 0; i < trans_per_thread; ++i) {
        query_type query;

        auto r = ud.sample();
        bool ro_txn = r < readonly_ceil;

        std::set<sampling::index_t> req_keys;

        while (1) {
            auto k = ud.sample() % ARRAY_SZ;
            if (k == 0)
                continue;
            req_keys.insert(k);
            if (req_keys.size() == (size_t)opspertrans)
                break;
        }

        if (ro_txn)
            query.emplace_back(OpType::read, 0);

        int idx = 0;
        for (auto it = req_keys.begin(); it != req_keys.end(); ++it) {
            RWOperation op;
            if (!ro_txn && idx >= write_thresh)
                op.type = OpType::write;
            else
                op.type = OpType::read;
            op.key = *it;
            if (op.type == OpType::write)
                op.value = op.key + 1;
            query.push_back(op);
            idx++;
        }

        assert((size_t)idx == req_keys.size());

        if (!ro_txn)
            query.emplace_back(OpType::write, 0, thread_id);

        thread_workload.push_back(query);
    }

    if (dump_trace)
        dump_thread_trace(thread_id, thread_workload);
}

template <int DS>
bool HotspotRW<DS>::prepopulate() {
    std::cout << "Generating workload..." << std::endl;
    workloads.resize(nthreads);

    std::vector<std::thread> thrs;
    for (int i = 0; i < nthreads; ++i)
        thrs.emplace_back(&HotspotRW::per_thread_workload_init, this, i);
    for (auto &t : thrs)
        t.join();

    std::cout << "Generation complete." << std::endl;

#if DEBUG_SKEW
    skew_account.resize(nthreads);
#endif

    return true;
}

template <int DS>
void HotspotRW<DS>::run(int me, uint64_t start_tsc) {
    TThread::set_id(me);
    container_type* a = this->a;
    container_type::thread_init(*a);


    auto &tw = workloads[me];

#if DEBUG_SKEW
    bool seen_stop = false;
    unsigned long start_tsc = read_tsc();
#endif

#if CHECK_PROGRESS
    static constexpr uint64_t delta_t = 10 * 2200000; // 10 ms checking interval
    uint64_t total_delta = delta_t;
    auto it_prev = tw.begin();
    uint64_t prev_total_reads = 0;
    uint64_t prev_total_opt_reads = 0;
#endif

    for (auto txn_it = tw.begin(); txn_it != tw.end(); ++txn_it) {
#if DEBUG_SKEW
        if (stop && !seen_stop) {
            auto ticks = read_tsc() - start_tsc;
            skew_account[me].ntxns_at_stop = txn_it - tw.begin();
            skew_account[me].time_to_stop = ticks;
            seen_stop = true;
        }
#endif
        TRANSACTION {
            for (auto &req : *txn_it) {
                switch (req.type) {
                case OpType::read: {
                    value_type r;
                    TXN_DO(doRead(*a, req.key, r));}
                    break;
                case OpType::write:
                    TXN_DO(doWrite(*a, req.key, req.value));
                    break;
                case OpType::inc: {
                    value_type r;
                    TXN_DO(doRead(*a, req.key, r));
                    ++r;
                    TXN_DO(doWrite(*a, req.key, r));
                    }
                    break;
                default:
                    std::cerr << "unkown OpType: " << (int)req.type << std::endl;
                    abort();
                    break;
                }
            }
        } RETRY(true);

#if CHECK_PROGRESS
        uint64_t now = read_tsc();
        if (now - start_tsc >= total_delta) {
            total_delta += delta_t;
            auto ntxns = txn_it - it_prev;
            it_prev = txn_it;

            auto total_reads = TXP_INSPECT(txp_total_r);
            auto total_opt_reads = TXP_INSPECT(txp_total_adaptive_opt);

            double opt_rate = (double)(total_opt_reads - prev_total_opt_reads) / double(total_reads - prev_total_reads);

            prev_total_reads = total_reads;
            prev_total_opt_reads = total_opt_reads;

            progress[me].push_back(ntxns);
            optimistic_rate[me].push_back(opt_rate);
        }
#endif
    }

#if DEBUG_SKEW
    skew_account[me].time_to_quota = read_tsc() - start_tsc;

    if (!stop) {
        bool_cmpxchg(&stop, false, true);
        if (!seen_stop) {
            skew_account[me].ntxns_at_stop = tw.size();
            skew_account[me].time_to_stop = skew_account[me].time_to_quota;
        }
    }
#endif
}

template <int DS>
void HotspotRW<DS>::report() {
#if DEBUG_SKEW
    std::vector<unsigned long> times_to_quota;
    std::stringstream ss;
    for (int i = 0; i < nthreads; ++i) {
        ss << "Thread " << i;
        ss << ": n=" << skew_account[i].ntxns_at_stop;
        ss << ", t_stop=" << (unsigned long)((double)skew_account[i].time_to_stop / PROC_TSC_FREQ);
        auto ttq = (unsigned long)((double)skew_account[i].time_to_quota / PROC_TSC_FREQ);
        times_to_quota.push_back(ttq);
        ss << ", t_quota=" << ttq;
        ss << std::endl;
    }
    ss << "Minimum time to quota: " << *std::min_element(times_to_quota.begin(), times_to_quota.end()) << std::endl;
    std::cout << ss.str() << std::flush;
#endif
#if CHECK_PROGRESS
    std::stringstream ss;
    ss << "Instantaneous throughput (ntxns), opt read rate (in %, every 10 ms):" << std::endl;
    size_t effective_length = progress[0].size();
    for (int i = 0; i < nthreads; ++i) {
        effective_length = std::min(effective_length, progress[i].size());
    }

    for (size_t idx = 0; idx < effective_length; ++idx) {
        uint64_t agg = 0;
        double avg_rate = 0;
        for (int t = 0; t < nthreads; ++t) {
            agg += progress[t][idx];
            avg_rate += optimistic_rate[t][idx];
        }

        ss << agg << ", " << avg_rate * 100 / nthreads << std::endl;
    }

    std::cout << ss.str() << std::flush;
#endif
}

template <int DS>
struct Hotspot2RW : public HotspotRW<DS> {
    typedef std::vector<RWOperation> query_type;
    void per_thread_workload_init(int thread_id) override;
};

template <int DS>
void Hotspot2RW<DS>::per_thread_workload_init(int thread_id) {
    sampling::StoUniformDistribution ud(thread_id, 0, std::numeric_limits<uint32_t>::max());

    auto& thread_workload = this->workloads[thread_id];

    int trans_per_thread = ntrans / nthreads;
    uint64_t readonly_ceil = (uint64_t)(readonly_percent * std::numeric_limits<uint32_t>::max());
    int write_thresh = (int)(write_percent * opspertrans);

    for (int i = 0; i < trans_per_thread; ++i) {
        query_type query;

        auto r = ud.sample();
        bool ro_txn = r < readonly_ceil;

        std::set<sampling::index_t> req_keys;

        while (1) {
            auto k = ud.sample() % ARRAY_SZ;
            if (k == 0)
                continue;
            req_keys.insert(k);
            if (req_keys.size() == (size_t)opspertrans)
                break;
        }

        if (!ro_txn)
            query.emplace_back(OpType::inc, 0);

        int idx = 0;
        for (auto it = req_keys.begin(); it != req_keys.end(); ++it) {
            RWOperation op;
            if (!ro_txn && idx >= write_thresh)
                op.type = OpType::write;
            else
                op.type = OpType::read;
            op.key = *it;
            if (op.type == OpType::write)
                op.value = op.key + 1;
            query.push_back(op);
            if (ro_txn && idx == (opspertrans/2))
                query.emplace_back(OpType::read, 0);
            idx++;
        }

        assert((size_t)idx == req_keys.size());
        assert(query.size() == (size_t)opspertrans + 1);

        //if (!ro_txn)
        //    query.emplace_back(OpType::write, 0, thread_id);

        thread_workload.push_back(query);
    }

    if (dump_trace)
        dump_thread_trace(thread_id, thread_workload);
}

// Test: SingleRW to test overhead of global TID in opacity
template <int DS>
struct SingleRW : public HotspotRW<DS> {
    typedef std::vector<RWOperation> query_type;
    void per_thread_workload_init(int thread_id) override;
};

template <int DS>
void SingleRW<DS>::per_thread_workload_init(int thread_id) {
    sampling::StoUniformDistribution ud(thread_id, 0, std::numeric_limits<uint32_t>::max());

    auto& thread_workload = this->workloads[thread_id];

    int trans_per_thread = ntrans/nthreads;

    for (int i = 0; i < trans_per_thread; ++i) {
        query_type query;
        query.emplace_back(OpType::inc, ud.sample() % ARRAY_SZ);
        thread_workload.push_back(query);
    }

    if (dump_trace)
        dump_thread_trace(thread_id, thread_workload);
}

// Test: ZipfRW
template <int DS>
struct ZipfRW : public HotspotRW<DS> {
    typedef std::vector<RWOperation> query_type;
    void per_thread_workload_init(int thread_id) override;
};

template <int DS>
void ZipfRW<DS>::per_thread_workload_init(int thread_id) {
    sampling::StoUniformDistribution ud(thread_id, 0, std::numeric_limits<uint32_t>::max());
    sampling::StoRandomDistribution *dd = nullptr;

    if (zipf_skew == 0.0)
        dd = new sampling::StoUniformDistribution(thread_id, 0, ARRAY_SZ-1);
    else
        dd = new sampling::StoZipfDistribution(thread_id, 0, ARRAY_SZ-1, zipf_skew);

    uint32_t ro_threshold = (uint32_t)(std::numeric_limits<uint32_t>::max() * readonly_percent);
    uint32_t write_threshold = (uint32_t)(std::numeric_limits<uint32_t>::max() * write_percent);

    auto& thread_workload = this->workloads[thread_id];
    int trans_per_thread = ntrans / nthreads;

    for (int i = 0; i < trans_per_thread; ++i) {
        query_type query;
        bool read_only = ud.sample() < ro_threshold;
        int nops = read_only ? opspertrans_ro : opspertrans;

        std::vector<sampling::index_t> idx_set;
        while (idx_set.size() != (size_t)nops) {
            idx_set.push_back(dd->sample());
        }

        for (auto idx : idx_set) {
            if (read_only)
                query.emplace_back(OpType::read, idx);
            else {
                if (ud.sample() < write_threshold)
                    query.emplace_back(OpType::write, idx, idx + 1);
                else
                    query.emplace_back(OpType::read, idx);
            }
        }
        thread_workload.push_back(query);
    }

    if (dump_trace)
        dump_thread_trace(thread_id, thread_workload);

    delete dd;
}


// Test: ReadThenWrite
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
  //TThread::set_always_allocate(readMyWrites);
  Sto::update_threadid();
#ifdef BOOSTING_STANDALONE
  boosting_threadid = me;
#endif
  container_type* a = this->a;
  container_type::thread_init(*a);

  //unsigned ids[100] = {10, 240, 45, 205, 80, 170, 105, 140, 11, 239, 46, 204, 81, 169, 106, 141, 12, 238, 47, 203, 82, 168, 107, 142, 13, 237, 48, 202, 83, 167, 108, 143, 14, 236, 49, 201, 84, 166, 109, 144, 15, 235, 50, 200, 85, 165, 110, 145, 16, 234, 51, 199, 86, 164, 111, 146, 17, 233, 52, 198, 87, 163, 112, 147, 18, 232, 53, 197, 88, 162, 113, 148, 19, 231, 54, 196, 89, 161, 114, 149, 20, 230, 55, 195, 90, 160, 115, 150, 21, 229, 56, 194, 91, 159, 116, 151, 22, 228, 57, 193}; 

#if NON_CONFLICTING
  long range = ARRAY_SZ/nthreads;
  std::uniform_int_distribution<long> slotdist(me*range + 10, (me + 1) * range - 1 - 10);
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
      bool success = true;
#if MAINTAIN_TRUE_ARRAY_STATE
      nslots_written = 0;
#endif
      transgen = transgen_snap;
#if ALL_UNIQUE_SLOTS
      bool used[ARRAY_SZ] = {false};
#endif

      for (int j = 0; j < OPS; ++j) {
        int slot  = slotdist(transgen);
        //slot = slot +  ids[j] * 4 - slot;
        //int slot = me * range + j;
#if ALL_UNIQUE_SLOTS
        while (used[slot]) slot = slotdist(transgen);
        used[slot]=true;
#endif
        //doRead(*a, slot);
        auto r = transgen();
        //if (do_delete && r > (write_thresh+write_thresh/2)) {
          // TODO: this doesn't make that much sense if write_percent != 50%
          //doDelete(*a, slot);
        //} else 
        if (r > write_thresh) {
          //doRead(*a, slot);
	  success = doWrite(*a, slot, j);
        } else {
          //doWrite(*a, slot, j);
          value_type r;
	  success = doRead(*a, slot, r);

#if MAINTAIN_TRUE_ARRAY_STATE
          slots_written[nslots_written++] = slot;
#endif
        }
        TXN_DO(success);
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
  std::cout << "Checked" << std::endl;
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
  int N = ntrans / nthreads;

  for (int j = 0; j < N; j++) {
  TRANSACTION {
    for (int i = 0; i < ARRAY_SZ; ++i) {
      if ((i % nthreads) >= me) {
        auto cur = a->transGet(i);
        //std::ofstream outfile;
        //outfile.open(std::to_string(me), std::ios_base::app);
        //outfile << "Transaction on thread [" << me << "] writes array  [" << i << "]" << std::endl;
        //outfile.close(); 
	a->transPut(i, val(unval(cur)+1));
      }
   }  
      //std::ofstream outfile;
      //outfile.open(std::to_string(me), std::ios_base::app);
      //outfile << "Transaction on thread [" << me << "] starts commit, number = [" << j << "]" << std::endl;
      //outfile.close();
 
  } RETRY(true);
  }
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
    uint64_t start_tsc;
};

void* runfunc(void* x) {
    TesterPair* tp = (TesterPair*) x;
    set_affinity(tp->me + 1);
    tp->t->run(tp->me, tp->start_tsc);
    return nullptr;
}

void startAndWait(int n, Tester* tester, uint64_t start_tsc) {
  pthread_t tids[n];
  TesterPair testers[n];
  for (int i = 0; i < n; ++i) {
      testers[i].t = tester;
      testers[i].me = i;
      testers[i].start_tsc = start_tsc;
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

void print_time(double time) {
  printf("%f\n", time);
}

#define MAKE_TESTER(name, desc, type, ...)            \
    {name, desc, 0, new type<0, ## __VA_ARGS__>},     \
    {name, desc, 10, new type<10, ## __VA_ARGS__>},   \
    {name, desc, 11, new type<11, ## __VA_ARGS__>},   \
    {name, desc, 12, new type<12, ## __VA_ARGS__>},   \
    {name, desc, 14, new type<14, ## __VA_ARGS__>}

//    {name, desc, 1, new type<1, ## __VA_ARGS__>},     
//    {name, desc, 2, new type<2, ## __VA_ARGS__>},     
//    {name, desc, 4, new type<4, ## __VA_ARGS__>},     
//    {name, desc, 7, new type<7, ## __VA_ARGS__>},     
//    {name, desc, 8, new type<8, ## __VA_ARGS__>},     
//    {name, desc, 9, new type<9, ## __VA_ARGS__>},     
//    {name, desc, 6, new type<6, ## __VA_ARGS__>},
//    {name, desc, 13, new type<13, ## __VA_ARGS__>},

struct Test {
    const char* name;
    const char* desc;
    int ds;
    Tester* tester;
} tests[] = {
//    MAKE_TESTER("isolatedwrites", 0, IsolatedWrites),
//    MAKE_TESTER("blindwrites", 0, BlindWrites),
//    MAKE_TESTER("interferingwrites", 0, InterferingRWs),
    MAKE_TESTER("randomrw", "typically best choice", RandomRWs, false),
//    MAKE_TESTER("readthenwrite", 0, ReadThenWrite),
//    MAKE_TESTER("kingofthedelete", 0, KingDelete),
//    MAKE_TESTER("xordelete", 0, XorDelete),
//    MAKE_TESTER("randomrw-d", "uncheckable", RandomRWs, true),
    MAKE_TESTER("hotspot", "contending hotspot", HotspotRW),
    MAKE_TESTER("hotspot2", "contending hotspot (less stupid)", Hotspot2RW),
    MAKE_TESTER("singlerw", "increment a single random element", SingleRW),
    MAKE_TESTER("zipfrw", "Zipf random rw", ZipfRW)
};

struct {
    const char* name;
    int ds;
} ds_names[] = {
    {"array", USE_ARRAY},
    {"array-nonopaque", USE_ARRAY_NONOPAQUE},
    {"array-adaptive", USE_ARRAY_ADAPTIVE},
    {"array-tictoc", USE_ARRAY_TICTOC},
    {"hashtable", USE_HASHTABLE},
    {"hash", USE_HASHTABLE},
    {"hash-str", USE_HASHTABLE_STR},
    {"masstree", USE_MASSTREE},
    {"mass", USE_MASSTREE},
    {"masstree-str", USE_MASSTREE_STR},
    {"tgeneric", USE_TGENERICARRAY},
    {"queue", USE_QUEUE},
//    {"vector", USE_VECTOR},
    {"tvector", USE_TVECTOR},
    {"swissarray", USE_SWISSARRAY}
//    {"swissgeneric", USE_SWISSGENERICARRAY}
};

enum {
    opt_test = 1, opt_nrmyw, opt_check, opt_profile, opt_dump, opt_nthreads, opt_ntrans, opt_opspertrans, opt_opspertrans_ro, opt_writepercent, opt_readonlypercent, opt_blindrandwrites, opt_prepopulate, opt_seed, opt_skew
};

static const Clp_Option options[] = {
  { "no-readmywrites", 'n', opt_nrmyw, 0, 0 },
  { "check", 'c', opt_check, 0, Clp_Negate },
  { "profile", 'p', opt_profile, 0, Clp_Negate},
  { "dump", 'd', opt_dump, 0, Clp_Negate},
  { "nthreads", 'j', opt_nthreads, Clp_ValInt, Clp_Optional },
  { "ntrans", 0, opt_ntrans, Clp_ValInt, Clp_Optional },
  { "opspertrans", 0, opt_opspertrans, Clp_ValInt, Clp_Optional },
  { "opspertrans_ro", 0, opt_opspertrans_ro, Clp_ValInt, Clp_Optional },
  { "writepercent", 0, opt_writepercent, Clp_ValDouble, Clp_Optional },
  { "readonlypercent", 0, opt_readonlypercent, Clp_ValDouble, Clp_Optional },
  { "blindrandwrites", 0, opt_blindrandwrites, 0, Clp_Negate },
  { "prepopulate", 0, opt_prepopulate, Clp_ValInt, Clp_Optional },
  { "seed", 's', opt_seed, Clp_ValUnsigned, 0 },
  { "skew", 0, opt_skew, Clp_ValDouble, Clp_Optional}
};

static void help(const char *name) {
  printf("Usage: %s test-number [OPTIONS]\n\
Options:\n\
 -n, --no-readmywrites\n\
 -c, --check, run a check of the results afterwards\n\
 -p, --profile, run a perf profile of the execution porition of the benchmark\n\
 -d, --dump, dump the workload executed by each thread (works only for hotspot (8))\n\
 --nthreads=NTHREADS (default %d)\n\
 --ntrans=NTRANS, how many total transactions to run (they'll be split between threads) (default %d)\n\
 --opspertrans=OPSPERTRANS, how many operations to run per transaction (default %d)\n\
 --opspertrans_ro=OPRP, how many operations in read only transactions (default to opspertrans)\n\
 --writepercent=WRITEPERCENT, probability with which to do writes versus reads (default %f)\n\
 --readonlypercent=ROPERCENT, probability with which a transaction is read-only (default %f)\n\
 --blindrandwrites, do blind random writes for random tests. makes checking impossible\n\
 --prepopulate=PREPOPULATE, prepopulate table with given number of items (default %d)\n\
 --seed=SEED\n\
 --skew=SKEW, skew parameter for zipfrw test type (default %f)\n",
         name, nthreads, ntrans, opspertrans, write_percent, readonly_percent, prepopulate, zipf_skew);
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

void time_and_run(double *real_time,
                  struct rusage* ru1, struct rusage* ru2,
                  int nthreads, Tester* tester) {
    unsigned long t1 = read_tsc();
    getrusage(RUSAGE_SELF, ru1);
    startAndWait(nthreads, tester, t1);
    unsigned long t2 = read_tsc();
    getrusage(RUSAGE_SELF, ru2);
    *real_time = ((double)(t2-t1)) / BILLION / PROC_TSC_FREQ;
    tester->report();
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
    case opt_profile:
      profile = !clp->negated;
      break;
    case opt_dump:
      dump_trace = !clp->negated;
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
    case opt_opspertrans_ro:
      opspertrans_ro = clp->val.i;
      break;
    case opt_writepercent:
      write_percent = clp->val.d;
      break;
    case opt_readonlypercent:
      readonly_percent = clp->val.d;
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
    case opt_skew:
        zipf_skew = clp->val.d;
        break;
    default:
      help(argv[0]);
    }
  }

  if (opspertrans_ro == -1)
    opspertrans_ro = opspertrans;

  Clp_DeleteParser(clp);

    if (seed)
        srandom(seed);
    else
        srandomdev();
    for (unsigned i = 0; i < arraysize(initial_seeds); ++i) {
        auto ra = random();
        //std::cout << ra << std::endl;
        initial_seeds[i] = ra;
    }
        //initial_seeds[i] = random();

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

  if (!strcmp(tests[test].name, "zipfrw") && (zipf_skew < 0.0 || zipf_skew >= 1000.0)) {
    printf("Please enter a skew parameter between 0 and 1000 (currently entered %f)\n", zipf_skew);
    exit(1);
  }

  if (profile) {
    printf("INFO: System profiler will be spawned after initialization.\n");
  }

  Tester* tester = tests[test].tester;
  tester->initialize();

  double real_time;
  struct rusage ru1,ru2;
  if (profile) {
    Profiler::profile([&]() {
        time_and_run(&real_time, &ru1, &ru2, nthreads, tester);
    });
  } else {
    time_and_run(&real_time, &ru1, &ru2, nthreads, tester);
  }

  tester->finalize();

#if !DATA_COLLECT
  printf("real time: ");
#endif
  print_time(real_time);
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
  if (!strcmp(tests[test].name, "zipfrw"))
    printf("  Zipf distribution parameter(s): zipf_skew = %f, read-only txn prob. = %f, write prob. = %f\n", zipf_skew, readonly_percent, write_percent);
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
          printf("%stotal_aborts: %llu (%llu aborts at commit time, %llu in observe, %llus due to write lock time-outs)\n CM::should_abort: %llu, CM::on_write: %llu, CM::on_rollback: %llu, CM::start: %llu\n Allocate new items: %llu, BV hit: %llu", sep, tc.p(txp_total_aborts), tc.p(txp_commit_time_aborts), tc.p(txp_observe_lock_aborts), tc.p(txp_lock_aborts), tc.p(txp_cm_shouldabort), tc.p(txp_cm_onwrite), tc.p(txp_cm_onrollback), tc.p(txp_cm_start), tc.p(txp_allocate), tc.p(txp_bv_hit));
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
