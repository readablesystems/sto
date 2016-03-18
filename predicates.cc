#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include <random>
#include <map>
#include "Transaction.hh"
#include "TVector.hh"
#include "TVector_nopred.hh"
#include "clp.h"
#include <sys/time.h>
int max_range = 10000;
int max_value = 1000;
int global_seed = 11;
int nthreads = 2;
int ntrans = 1000000;
int opspertrans = 1;
int prepopulate = 1000;
int max_key = 1000;
double search_percent = 0.6;
double pushback_percent = 0.2; // pop_percent will be the same to keep the size of array roughly the same
bool dumb_iterator = false;

int find_aborts[16];
int unsuccessful_finds = 0;
TransactionTid::type lock;

typedef TVector<int> pred_data_structure;
typedef TVector_nopred<int> nopred_data_structure;

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

template <typename T>
struct TesterPair {
  T* t;
  int me;
};

template<class InputIt, class T>
InputIt findIt(InputIt first, InputIt last, const T& value)
{
  for (; first != last; ++first) {
    if (*first == value) {
      return first;
    }
  }
  return last;
}

template <typename T>
int findK(T* q, int val) {
  typename T::const_iterator fit = q->cbegin();
  typename T::const_iterator eit = q->cend();
  typename T::const_iterator it = findIt(fit, eit, val);
  return it-fit;
}

template <typename T>
void run(T* q, int me) {
  TThread::set_id(me);
  
  std::uniform_int_distribution<long> slotdist(0, max_range);
  int N = ntrans/nthreads;
  int OPS = opspertrans;
  bool find_op = false;
  for (int i = 0; i < N; ++i) {
    // so that retries of this transaction do the same thing
    auto transseed = i;
    while (1) {
      Sto::start_transaction();
      try {
        uint32_t seed = transseed*3 + (uint32_t)me*ntrans*7 + (uint32_t)global_seed*MAX_THREADS*ntrans*11;
        auto seedlow = seed & 0xffff;
        auto seedhigh = seed >> 16;
        Rand transgen(seed, seedlow << 16 | seedhigh);
        for (int j = 0; j < OPS; ++j) {
          find_op = false;
          int op = slotdist(transgen) % 100;
          if (op < search_percent * 100) {
            int val = slotdist(transgen) % int(max_key * 0.5); // This is always a successful find
            find_op = true;
            findK(q, val);
          } else if (op < (search_percent + pushback_percent) *100){
            int sz = q->size();
            if (sz < 1.1 * max_key) {
              int val = slotdist(transgen) % max_value;
              q->push_back(val);
            } else {
              q->pop_back();
            }
          } else if (op < (search_percent + 2*pushback_percent) * 100) {
            int sz = q->size();
            if (sz > 0.9*max_key) {
              q->pop_back();
            } else {
              int val = slotdist(transgen) % max_value;
              q->push_back(val);
            }
          } else {
            int sz = q->size();
            int key = slotdist(transgen) % (sz - 50);
            
            q->transGet(key);
          }
          
        }
        
        if (Sto::try_commit()) {
          break;
        }
        
      } catch (Transaction::Abort e) {
        if (find_op) {
          find_aborts[me]++;
        }
      }
    }
  }
}

template <typename T>
void* runFunc(void* x) {
  TesterPair<T>* tp = (TesterPair<T>*) x;
  run(tp->t, tp->me);
  return nullptr;
}

template <typename T>
void startAndWait(int n, T* queue) {
  pthread_t tids[nthreads];
  TesterPair<T> *testers = new TesterPair<T>[n];
  for (int i = 0; i < nthreads; ++i) {
    testers[i].t = queue;
    testers[i].me = i;
    pthread_create(&tids[i], NULL, runFunc<T>, &testers[i]);
  }
  pthread_t advancer;
  pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
  pthread_detach(advancer);
  
  for (int i = 0; i < nthreads; ++i) {
    pthread_join(tids[i], NULL);
  }
}

void print_time(struct timeval tv1, struct timeval tv2) {
  printf("%f\n", (tv2.tv_sec-tv1.tv_sec) + (tv2.tv_usec-tv1.tv_usec)/1000000.0);
}

enum {
  opt_nthreads, opt_ntrans, opt_opspertrans, opt_searchpercent, opt_pushbackpercent, opt_prepopulate, opt_seed
};

static const Clp_Option options[] = {
  { "nthreads", 0, opt_nthreads, Clp_ValInt, Clp_Optional },
  { "ntrans", 0, opt_ntrans, Clp_ValInt, Clp_Optional },
  { "opspertrans", 0, opt_opspertrans, Clp_ValInt, Clp_Optional },
  { "searchpercent", 0, opt_searchpercent, Clp_ValDouble, Clp_Optional },
  { "pushbackpercent", 0, opt_pushbackpercent, Clp_ValDouble, Clp_Optional },
  { "prepopulate", 0, opt_prepopulate, Clp_ValInt, Clp_Optional },
  { "seed", 0, opt_seed, Clp_ValInt, Clp_Optional },
};

static void help() {
  printf("Usage: [OPTIONS]\n\
         Options:\n\
         --nthreads=NTHREADS (default %d)\n\
         --ntrans=NTRANS, how many total transactions to run (they'll be split between threads) (default %d)\n\
         --opspertrans=OPSPERTRANS, how many operations to run per transaction (default %d)\n\
         --searchpercent=SEARCHPERCENT, probability with which to do searches (default %f)\n\
         --pushbackpercent=PUSHBACKPERCENT, probability with which to do push backs (default %f)\n\
         --prepopulate=PREPOPULATE, prepopulate table with given number of items (default %d)\n\
         --seed=SEED, global seed to run the experiment \n",
         nthreads, ntrans, opspertrans, search_percent, pushback_percent, prepopulate);
  exit(1);
}

template <typename T>
void init(T* q) {
  std::uniform_int_distribution<long> slotdist(0, max_range);
  uint32_t seed = global_seed * 11;
  auto seedlow = seed & 0xffff;
  auto seedhigh = seed >> 16;
  Rand transgen(seed, seedlow << 16 | seedhigh);
  for (int i = 0; i < prepopulate; i++) {
    TRANSACTION {
      q->push_back(i);//slotdist(transgen) % max_value);
    } RETRY(false);
  }
}

int main(int argc, char *argv[]) {
  lock = 0;
  struct timeval tv1,tv2;
  
  Clp_Parser *clp = Clp_NewParser(argc, argv, arraysize(options), options);
  
  int opt;
  while ((opt = Clp_Next(clp)) != Clp_Done) {
    switch (opt) {
      case opt_nthreads:
        nthreads = clp->val.i;
        break;
      case opt_ntrans:
        ntrans = clp->val.i;
        break;
      case opt_opspertrans:
        opspertrans = clp->val.i;
        break;
      case opt_searchpercent:
        search_percent = clp->val.d;
        break;
      case opt_pushbackpercent:
        pushback_percent = clp->val.d;
        break;
      case opt_prepopulate:
        prepopulate = clp->val.i;
        break;
      case opt_seed:
        global_seed = clp->val.i;
        break;
      default:
        help();
    }
  }
  Clp_DeleteParser(clp);
  
  dumb_iterator = false;
  for (int i = 0; i < 16; i++) {
    find_aborts[i] = 0;
  }

  unsuccessful_finds = 0;
  pred_data_structure q;
  q.nontrans_reserve(4096);
  init(&q);
  
  gettimeofday(&tv1, NULL);
  
  startAndWait(nthreads, &q);
  
  gettimeofday(&tv2, NULL);
  int total_aborts = 0;
  for (int i = 0; i < 16; i++) {
    total_aborts += find_aborts[i];
  }
  printf("Find aborts: %i, unsuccessful finds: %i\n", total_aborts, unsuccessful_finds);
  printf("Predicates: ");
  print_time(tv1, tv2);
  
#if STO_PROFILE_COUNTERS
  Transaction::print_stats();
  {
    txp_counters tc = Transaction::txp_counters_combined();
    printf("total_n: %llu, total_r: %llu, total_w: %llu, total_searched: %llu, total_aborts: %llu (%llu aborts at commit time)\n", tc.p(txp_total_n), tc.p(txp_total_r), tc.p(txp_total_w), tc.p(txp_total_searched), tc.p(txp_total_aborts), tc.p(txp_commit_time_aborts));
  }
  Transaction::clear_stats();
#endif
  dumb_iterator = true;
  for (int i = 0; i < 16; i++) {
    find_aborts[i] = 0;
  }
  unsuccessful_finds = 0;
  nopred_data_structure q2;
  q2.nontrans_reserve(4096);
  
  init(&q2);
  gettimeofday(&tv1, NULL);
  
  startAndWait(nthreads, &q2);
  
  gettimeofday(&tv2, NULL);
  total_aborts = 0;
  for (int i = 0; i < 16; i++) {
    total_aborts += find_aborts[i];
  }
  printf("Find aborts: %i, unsuccessful finds: %i\n", total_aborts, unsuccessful_finds);
  printf("No predicates: ");
  print_time(tv1, tv2);
  
#if STO_PROFILE_COUNTERS
  Transaction::print_stats();
  {
    txp_counters tc = Transaction::txp_counters_combined();
    printf("total_n: %llu, total_r: %llu, total_w: %llu, total_searched: %llu, total_aborts: %llu (%llu aborts at commit time)\n", tc.p(txp_total_n), tc.p(txp_total_r), tc.p(txp_total_w), tc.p(txp_total_searched), tc.p(txp_total_aborts), tc.p(txp_commit_time_aborts));
  }
  Transaction::clear_stats();
#endif
  
	return 0;
}
