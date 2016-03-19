#include <iostream>
#include <assert.h>
#include <random>
#include <climits>
#include <sys/time.h>
#include <sys/resource.h>

#include "Transaction.hh"
#include "clp.h"
#include "Queue.hh"
#include "randgen.hh"

// size of queue
#define QUEUE_SZ 4096

// only used for randomRWs test
#define GLOBAL_SEED 0
#define TRY_READ_MY_WRITES 0
#define MAINTAIN_TRUE_ARRAY_STATE 1

// if true, each operation of a transaction will act on a different slot
#define DATA_COLLECT 0
#define RANDOM_REPORT 0
#define STRING_VALUES 0
#define UNBOXED_STRINGS 0

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

typedef Queue<value_type> QueueType;
QueueType* q;
QueueType* q2;

bool readMyWrites = true;
bool runCheck = false;
int nthreads = 4;
int ntrans = 1000000;
int opspertrans = 10;
int prepopulate = QUEUE_SZ/8;
double write_percent = 0.5;
bool blindRandomWrite = false;

using namespace std;

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

static void doRead(Transaction& t) {
  if (readMyWrites) {
    value_type v;
    q->transFront(t, v);
    q->transPop(t);
  }
}

static void doWrite(Transaction& t, int& ctr) {
    q->transPush(t, val(ctr));
    ++ctr; // because we've done a read and a write
}
  
static inline void nreads(int n, Transaction& t) {
  for (int i = 0; i < n; ++i) {
    doRead(t);
  }
}
static inline void nwrites(int n, Transaction& t) {
  for (int i = 0; i < n; ++i) {
    doWrite(t, i);
  }
}

void *randomRWs(void *p) {
  int me = (intptr_t)p;
  TThread::id = me;
  
  // randomness to determine write or read (push or pop)
  uint32_t write_thresh = (uint32_t) (write_percent * Rand::max());

  int N = ntrans/nthreads;
  int OPS = opspertrans;
  for (int i = 0; i < N; ++i) {
    // so that retries of this transaction do the same thing
    auto transseed = i;
    
    bool done = false;
    while (1) {
      try{
      uint32_t seed = transseed*3 + (uint32_t)me*N*7 + (uint32_t)GLOBAL_SEED*MAX_THREADS*N*11;
      auto seedlow = seed & 0xffff;
      auto seedhigh = seed >> 16;
      Rand transgen(seed, seedlow << 16 | seedhigh);

      Transaction t;
      for (int j = 0; j < OPS; ++j) {
        // can call transgen to generate numbers 0-randmax
        auto r = transgen();
        if (r > write_thresh) {
          doRead(t);
        } else {
          doWrite(t, j);
        }
      }
      if (t.commit())
        break;
      } catch (Transaction::Abort E) {}
      if (!done) {
        debug("thread%d retrying\n", me);
      }
    }
  }
  return NULL;
}

void checkRandomRWs() {
    QueueType *old = q;
    QueueType check;
    q = &check;

    for (int i = 0; i < prepopulate; ++i) {
        Transaction t;
        q->transPush(t, val(0));
        t.commit();
    }
    
    for (int i = 0; i < nthreads; ++i) {
        randomRWs((void*)(intptr_t)i);
    }

    q = old;
    while (!q->empty()) {
        if (unval(q->pop()) != unval(check.pop()))
            fprintf(stderr, "parallel %d, sequential %d\n", unval(q->pop()), unval(check.pop()));
    }
    assert(check.empty() == q->empty());
}


void *xorDelete(void *p) {
  int me = (intptr_t)p;
  TThread::id = me;

  int N = ntrans/nthreads;
  int OPS = opspertrans;

  if (me == 0) {
    // populate
    Transaction t;
    for (int i = 0; i < prepopulate; ++i) {
      q->transPush(t, val(i));
    }
    assert(t.commit());
  } else {
        // wait for populated
        while (q->empty()) {}
  }

  for (int i = 0; i < N; ++i) {
    while (1) {
      try {
        Transaction t;
        for (int j = 0; j < OPS; ++j) {
          value_type v = val(1);
          if (!q->transPop(t)) {
            // we pop if the q is nonempty, push if it's empty
            q->transPush(t, v);
          }
        }
        if (t.commit())
            break;
      } catch (Transaction::Abort E) {}
    }
  }
  return NULL;
}

void checkXorDelete() {
  QueueType *old = q;
  QueueType check;
  q = &check;
  
  for (int i = 0; i < nthreads; ++i) {
    xorDelete((void*)(intptr_t)i);
  }
  q = old;
  
  while (!q->empty()) {
    if (unval(q->pop()) != unval(check.pop()))
        fprintf(stderr, "parallel %d, sequential %d\n", unval(q->pop()), unval(check.pop()));
  }
  assert(check.empty() == q->empty());
}


void *queueTransfer(void *p) {
  int me = (intptr_t)p;
  TThread::id = me;

  int N = ntrans/nthreads;
  int OPS = opspertrans;

  if (me == 0) {
    // populate
    Transaction t;
    for (int i = 0; i < prepopulate; ++i) {
      q->transPush(t, val(i));
    }
    assert(t.commit());
  } else {
        // wait for populated
        while (q->empty()) {}
  }

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
        if (t.commit())
            break;
      } catch (Transaction::Abort E) {}
    }
  }
  return NULL;
}

void checkQueueTransfer() {
  // prepopulate
  for (int i = 0; i < prepopulate; ++i) {
      q->push(val(i));
  }
  
  for (int i = 0; i < nthreads; ++i) {
    queueTransfer((void*)(intptr_t)i);
  }
  
  // prepopulate
  for (int i = 0; i < prepopulate; ++i) {
      q->push(val(i));
  }
  
  while (!q->empty()) {
    if (unval(q->pop()) != unval(q2->pop()))
        fprintf(stderr, "parallel %d, sequential %d\n", unval(q->pop()), unval(q2->pop()));
  }
  assert(q->empty() == q2->empty());
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
  {randomRWs, checkRandomRWs},
  {xorDelete, checkXorDelete},
  {queueTransfer, checkQueueTransfer}
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
/*  printf("Ran test %d with: ARRAY_SZ: %d, readmywrites: %d, result check: %d, %d threads, %d transactions, %d ops per transaction, %f%% writes, blindrandwrites: %d\n\
 MAINTAIN_TRUE_ARRAY_STATE: %d, LOCAL_VECTOR: %d, SPIN_LOCK: %d, INIT_SET_SIZE: %d, GLOBAL_SEED: %d, TRY_READ_MY_WRITES: %d, STO_PROFILE_COUNTERS: %d\n",
         test, ARRAY_SZ, readMyWrites, runCheck, nthreads, ntrans, opspertrans, write_percent*100, blindRandomWrite,
         MAINTAIN_TRUE_ARRAY_STATE, LOCAL_VECTOR, SPIN_LOCK, INIT_SET_SIZE, GLOBAL_SEED, TRY_READ_MY_WRITES, STO_PROFILE_COUNTERS); */
#endif

#if STO_PROFILE_COUNTERS
#define LLU(x) ((long long unsigned)x)
  printf("total_n: %llu, total_r: %llu, total_w: %llu, total_searched: %llu, total_aborts: %llu (%llu aborts at commit time)\n", LLU(Transaction::total_n), LLU(Transaction::total_r), LLU(Transaction::total_w), LLU(Transaction::total_searched), LLU(Transaction::total_aborts), LLU(Transaction::commit_time_aborts));
#if MASSTREE
  printf("node aborts: %llu\n", LLU(node_aborts));
#endif
#undef LLU
#endif

  if (runCheck)
    tests[test].checkfunc();
}
