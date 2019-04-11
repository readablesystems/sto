#include <iostream>

#include "clp.h"
#include "PlatformFeatures.hh"
#include "Transaction.hh"
#include "DB_index.hh"
#include "DB_params.hh"
#include "DB_profiler.hh"

#include <limits>
#include <thread>
#include <vector>

#ifndef SHORT_CONCURRENT_TWO_PHASE_EXPERIMENT // DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
// Change this value to change the size of each index in the experiment.
#define NUM_ITEMS 10000
// Meant to capture the whole range of items in a database. Since keys are
// sorted alphanumerically, we need this (at least for debugging).
#define UPPER_BOUND_KEY "99999"
#define DEFAULT_NUM_THREADS 3
#define DEFAULT_NUM_WAREHOUSES 3
#define TIME_LIMIT 60 // 10
#define NUM_ITEM_ORDERS 300
#else
#define NUM_ITEMS 1000
#define UPPER_BOUND_KEY "99999"
#define DEFAULT_NUM_THREADS 1
#define DEFAULT_NUM_WAREHOUSES 1
#define TIME_LIMIT 60
// you left off here - for some reason, this value will stop all commit time
// aborts, but when you increase it to 129 items, there's a lot of commit time
// aborts.
#define NUM_ITEM_ORDERS 128
#endif

enum trans_type { DEFAULT, TWO_PHASE };

#define DEFAULT_TRANS TWO_PHASE



struct row {
  enum class NamedColumn : int {name = 0};
  int quantity;
  int ordered;

  row(int quantity, int ordered) : quantity(quantity), ordered(ordered) {}

  std::string to_string() const {
    return "{ quantity=" + std::to_string(quantity) + ", ordered=" +
      std::to_string(ordered) + " }";
  }
};

// Psuedo-random number generator
#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
static std::mt19937 gen(0);
#else
static std::mt19937 gen(time(NULL));
#endif

typedef std::string item_key;
typedef row item_value;

// clp parser definitions
enum {
    opt_nthreads, opt_nwarehouses, opt_ttrans
};

static const Clp_Option options[] = {
    { "nthreads", 'h', opt_nthreads, Clp_ValInt, Clp_Optional},
    { "nwarehouses", 'w', opt_nwarehouses, Clp_ValInt, Clp_Optional},
    { "trans_type", 't', opt_ttrans, Clp_ValString, Clp_Optional }
};

static inline void print_usage(const char *argv_0);

template <typename DBParams>
class concurrent_warehouse_experiment {
public:
  typedef bench::unordered_index<item_key, item_value, DBParams> concurrent_warehouse_type;
  typedef std::vector<concurrent_warehouse_type> warehouse_list_type;
  static const bench::RowAccess default_access = bench::RowAccess::ObserveValue;
  static const bench::RowAccess read_for_update_access = bench::RowAccess::UpdateValue;

  struct lookup_subaction_ret {
    bool success;
    bool found;
    uintptr_t row_ptr;
    const item_value* value;
    int subaction_id;

    lookup_subaction_ret(std::tuple<bool, bool, uintptr_t, const item_value*, int> sel_return) {
      std::tie(success, found, row_ptr, value, subaction_id) = sel_return;
    }
  };

  struct lookup_ret {
    bool success;
    bool found;
    uintptr_t row_ptr;
    const item_value* value;

    lookup_ret() {}
    lookup_ret(std::tuple<bool, bool, uintptr_t, const item_value*> sel_return) {
      std::tie(success, found, row_ptr, value) = sel_return;
    }
  };

private:
  static int parse_and_set_vars(int argc, const char* const* argv,
                                int& num_threads, int& num_warehouses,
                                trans_type& trans) {
    Clp_Parser* clp = Clp_NewParser(argc, argv, arraysize(options), options);
    int opt;
    int ret = 0;
    bool clp_stop = false;
    while (!clp_stop && ((opt = Clp_Next(clp)) != Clp_Done)) {
      switch (opt) {
      case opt_nthreads:
        num_threads = clp->val.i;
        break;
      case opt_nwarehouses:
        num_warehouses = clp->val.i;
        break;
      case opt_ttrans:
        if (strcmp(clp->val.s, "2pt") == 0 || strcmp(clp->val.s, "two_phase") == 0) {
          trans = TWO_PHASE;
        }
        else if (strcmp(clp->val.s, "default") == 0) {
          trans = DEFAULT;
        }
        else {
          printf("input for transaction type received but not one of "
                 "\"2pt\", \"two_phase\", or \"default\"\n");
        }
        break;
      default:
        print_usage(argv[0]);
        ret = 1;
        clp_stop = true;
        break;
      }
    }
    Clp_DeleteParser(clp);
    return ret;
  }

  static uint64_t random_dist(uint64_t x, uint64_t y) {
    std::uniform_int_distribution<uint64_t> dist(x, y);
    return dist(gen);
  }

  static std::string random_a_string(int x, int y) {
    size_t len = random_dist(x, y);
    std::string str;
    str.reserve(len);

    for (auto i = 0u; i < len; ++i) {
      auto n = random_dist(0, 61);
      char c = (n < 26) ? char('a' + n) :
        ((n < 52) ? char('A' + (n - 26)) : char('0' + (n - 52)));
      str.push_back(c);
    }
    return str;
  }

  static void insert_random_item(concurrent_warehouse_type& index, item_key key) {
    item_value value = item_value(random_dist(0, 100), random_dist(0, 100));
    index.nontrans_put(key, value);
  }

  static void prepopulate_database(long num_items, unsigned int num_warehouses,
                                   warehouse_list_type& warehouses) {
    for (unsigned i = 0; i < num_warehouses; i++) {
      concurrent_warehouse_type* warehouse = new concurrent_warehouse_type(num_items);
      // Insert random items into each 'warehouse'
      for (long j = 0; j < num_items; j++) {
        insert_random_item(*warehouse, std::to_string(j));
      }
      warehouses.push_back(*warehouse);
    }
  }

  static std::string thread_id_string() {
    return "[" + std::to_string(TThread::id()) + "] ";
  }

  static void debug_print(std::string s) {
    std::cout << thread_id_string() << s << std::endl;
  }

  static void newline() {
    std::cout << std::endl;
  }

  template <typename T, size_t N>
  static void debug_print_array(std::string array_name, T (&array)[N]) {
    std::cout << thread_id_string() << array_name << " { ";
    for (int i = 0; i < N; i++) {
      std::cout << array[i] << " ";
    }
    std::cout << " }" << std::endl;
  }

  static void run_two_phase_transaction(warehouse_list_type& warehouses) {
    TRANSACTION {
      Sto::start_two_phase_transaction();

#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
      debug_print("Two phase transaction starting");
#endif

      // Select items to order by randomly generating keys.
      // TODO: make sure that there are no duplicate keys in the list.
      item_key order_keys[NUM_ITEM_ORDERS];
      for (int i = 0; i < NUM_ITEM_ORDERS; i++) {
#ifndef CONTROLLED_EXPERIMENT_TWO_PHASE
        // order_keys[i] = std::to_string(random_dist(0, NUM_ITEMS - 1));
        order_keys[i] = std::to_string(i);
#else
        order_keys[i] = std::to_string(i);
#endif
      }

      // Get metrics for each warehouse
      int warehouse_metrics[warehouses.size()];
      int best_warehouse_index = -1;
      int best_score = std::numeric_limits<int>::min();
      int best_subaction_id = -1;

      for (int i = 0; i < warehouses.size(); i++) {
        int curr_score = 0;
        int curr_subaction_id = -1;

        // Metric for amount of items left in supply
        for (int key_i = 0; key_i < NUM_ITEM_ORDERS; key_i++) {
          lookup_ret ret;
          if (key_i == 0) {
            // Create a subaction for the first element
            lookup_subaction_ret sret =
              lookup_subaction_ret(warehouses[i].select_row_subaction(order_keys[0]));

#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
            // This assert also catches conflicts between two threads running
            // transactions, so it's commented out.
            // always_assert(sret.subaction_id != -1 && "subaction error");
#endif

            curr_subaction_id = sret.subaction_id;
            ret = lookup_ret(std::tuple<bool, bool, uintptr_t, const item_value*>
                             (sret.success, sret.found, sret.row_ptr, sret.value));
          }
          else {
            ret = lookup_ret(warehouses[i].select_row(order_keys[key_i]));
          }

          // If the lookup doesn't succeed, abort the transaction
          TXN_DO(ret.success);
#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
          always_assert(ret.found && "item not found or aborted");
#endif
          int warehouse_supply_metric = ret.value->quantity - ret.value->ordered;
          curr_score += warehouse_supply_metric;
        }

        // Metric for business of warehouse
        int warehouse_busy_metric = random_dist(0, 50);
        curr_score -= warehouse_busy_metric;

        // Metric for distance of warehouse
        int warehouse_distance_metric = random_dist(0, 50);
        curr_score -= warehouse_distance_metric;

#ifndef CONTROLLED_EXPERIMENT_TWO_PHASE
#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
        debug_print("current score is " + std::to_string(curr_score) +
                    ". best score is " + std::to_string(best_score));
#endif
        // Select the warehouse with the highest metric. Abort subactions of any
        // items not in best warehouse.
        if (curr_score > best_score) {
          best_score = curr_score;
          if (best_warehouse_index != -1) {
#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
            debug_print("curr score better than best score. Aborting subaction id " +
                        std::to_string(best_warehouse_index));
#endif
            Sto::abort_subaction(best_subaction_id);
          }
          best_subaction_id = curr_subaction_id;
          best_warehouse_index = i;

#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
          debug_print("current score is larger than best score.");
          debug_print("new best score is " + std::to_string(best_score) +
                      " with subaction id " + std::to_string(best_subaction_id) +
                      " and warehouse index " + std::to_string(best_warehouse_index));
          // debug_print_array<item_key, NUM_ITEM_ORDERS>("order keys:", order_keys);
          // debug_print_array<const item_value*, NUM_ITEM_ORDERS>("best order values:", best_order_values);
          // debug_print_array<uintptr_t, NUM_ITEM_ORDERS>("best order rowptrs:", best_order_rowptrs);
#endif
        }
        else {
          Sto::abort_subaction(curr_subaction_id);
        }
#else
        // Controlled experiment: have each thread run on a different
        // warehouse. 2PT shouldn't have any aborts.
        int runner_id = ::TThread::id();
        if (i != runner_id) {
          Sto::abort_subaction(curr_subaction_id);
        }
        else {
          best_subaction_id = curr_subaction_id;
          best_warehouse_index = i;
        }
#endif
      }

#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
        debug_print("Thread chose warehouse " + std::to_string(best_warehouse_index));
#endif

      Sto::phase_two();

      // TODO: make sure that only one subaction is still running at this point.
#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
      if (Sto::num_active_subactions() != 1) {
        debug_print("Expected one subaction running, found " +
                    std::to_string(Sto::num_active_subactions()));
      }
      always_assert(Sto::num_active_subactions() == 1);
#endif

      // Read the value of each item in the warehouse we chose.  Then update the
      // item, incrementing the amount ordered by 1.
      for (int key_i = 0; key_i < NUM_ITEM_ORDERS; key_i++) {
        lookup_ret ret = lookup_ret(warehouses[best_warehouse_index].select_row(order_keys[key_i], true));
        TXN_DO(ret.success);
#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
        always_assert(ret.found && "item not found or aborted");
#endif
        item_value new_value = { ret.value->quantity,
                                 ret.value->ordered + 1 };
        warehouses[best_warehouse_index].update_row(ret.row_ptr, &new_value);
      }

      // TODO: write more items in the second phase, ones you weren't looking for.

#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
      debug_print("Two phase transaction finished!");
      newline();
      newline();
#endif
    } RETRY (true);
  }

  static void run_default_transaction(warehouse_list_type& warehouses) {
    TRANSACTION {
#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
      debug_print("Default transaction starting");
#endif

      // Make an order by randomly generating item keys.
      // TODO: make sure that there are no duplicate keys in the list.
      item_key order_keys[NUM_ITEM_ORDERS];
      for (int i = 0; i < NUM_ITEM_ORDERS; i++) {
#ifndef CONTROLLED_EXPERIMENT_TWO_PHASE
        // order_keys[i] = std::to_string(random_dist(0, NUM_ITEMS - 1));
        order_keys[i] = std::to_string(i);
#else
        order_keys[i] = std::to_string(i);
#endif
      }

      // Get metrics for each warehouse
      int warehouse_metrics[warehouses.size()];
      int best_warehouse_index = -1;
      int best_score = std::numeric_limits<int>::min();

      for (int i = 0; i < warehouses.size(); i++) {
        int curr_score = 0;

        // Metric for amount of items left in supply.
        for (int key_i = 0; key_i < NUM_ITEM_ORDERS; key_i++) {
          lookup_ret ret =
            lookup_ret(warehouses[i].select_row(order_keys[key_i]));
          // If the lookup doesn't succeed, abort the transaction
          TXN_DO(ret.success);
#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
          always_assert(ret.found &&
                        "item not found or aborted. Maybe there was a thread conflict?");
#endif
          int warehouse_supply_metric = ret.value->quantity - ret.value->ordered;
          curr_score += warehouse_supply_metric;
        }

        // Metric for business of warehouse.
        int warehouse_busy_metric = random_dist(0, 50);
        curr_score -= warehouse_busy_metric;

        // Metric for distance of warehouse.
        int warehouse_distance_metric = random_dist(0, 50);
        curr_score -= warehouse_distance_metric;

#ifndef CONTROLLED_EXPERIMENT_TWO_PHASE
#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
        debug_print("current score is " + std::to_string(curr_score) +
                    ". best score is " + std::to_string(best_score));
#endif
        // Select the warehouse with the highest metric.
        if (curr_score > best_score) {
          best_score = curr_score;
          best_warehouse_index = i;

#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
          debug_print("current score is larger than best score.");
          debug_print("new best score is " + std::to_string(best_score) +
                      " with warehouse index " + std::to_string(best_warehouse_index));
          // debug_print_array<item_key, NUM_ITEM_ORDERS>("order keys:", order_keys);
          // debug_print_array<const item_value*, NUM_ITEM_ORDERS>("best order values:", best_order_values);
          // debug_print_array<uintptr_t, NUM_ITEM_ORDERS>("best order rowptrs:", best_order_rowptrs);
#endif
        }
#else
        // Controlled experiment: have each thread run on a different
        // warehouse. Default transaction should have some aborts.
        int runner_id = ::TThread::id();
        if (i == runner_id) {
          best_warehouse_index = i;
        }
#endif
      }

#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
        debug_print("Thread chose warehouse " + std::to_string(best_warehouse_index));
#endif

      // Read the value of each item in the warehouse we chose.  Then update the
      // item, incrementing the amount ordered by 1.
      for (int key_i = 0; key_i < NUM_ITEM_ORDERS; key_i++) {
        lookup_ret ret = lookup_ret(warehouses[best_warehouse_index].select_row(order_keys[key_i], true));
        TXN_DO(ret.success);
#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
        always_assert(ret.found && "item not found or aborted");
#endif
        item_value new_value = { ret.value->quantity,
                                 ret.value->ordered + 1 };
        warehouses[best_warehouse_index].update_row(ret.row_ptr, &new_value);
      }

      // TODO: write more items in the second phase, ones you weren't looking for.

#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
      debug_print("Default transaction finished!");
      newline();
      newline();
#endif
    } RETRY (true);
  }

    static void experiment_thread(warehouse_list_type& warehouses,
                                  bench::db_profiler& prof, int runner_id,
                                  double time_limit, uint64_t& txn_cnt,
                                  void (*transaction_method)(warehouse_list_type&)) {
      uint64_t local_txn_cnt = 0;
      ::TThread::set_id(runner_id);

      // TODO: uncomment when running on evaluation machine.
      // set_affinity(runner_id);

      uint64_t tsc_diff =
        (uint64_t)(time_limit * db_params::constants::processor_tsc_frequency *
                   db_params::constants::billion);
      //auto start_t = prof.start_timestamp();
      auto start_t = read_tsc();

      // Keep running transactions until time limit is reached.
      while(true) {
        auto curr_t = read_tsc();
        if ((curr_t - start_t) >= tsc_diff) {
          break;
        }
        transaction_method(warehouses);
        local_txn_cnt++;
      }

      txn_cnt = local_txn_cnt;

      std::cout << "runner " << runner_id << " finished" << std::endl;
    }

    static int run_benchmark(warehouse_list_type& warehouses,
                             bench::db_profiler& prof,
                             int num_threads, double time_limit,
                             trans_type trans) {
      std::vector<std::thread> threads;
      std::vector<uint64_t> txn_cnts(size_t(num_threads), 0);

      // initialize the threads for all the warehouses TODO: not sure if this is
      // right - I think it's causing an error because STO calls the thread_init
      // method when using the TransactionGuard class...
      // for (size_t i = 0; i < warehouses.size(); i++) {
      //   warehouses[i].thread_init();
      // }

      void (*transaction_method)(warehouse_list_type&);
      switch(trans) {
      case DEFAULT:
        transaction_method = &run_default_transaction;
        break;
      case TWO_PHASE:
        transaction_method = &run_two_phase_transaction;
        break;
      }
    
      for (int i = 0; i < num_threads; i++) {
        std::cout << "starting runner " << i << std::endl;
        threads.emplace_back(experiment_thread, std::ref(warehouses),
                             std::ref(prof), i, time_limit, std::ref(txn_cnts[i]),
                             transaction_method);
      }

      for (auto& t : threads) {
        t.join();
      }

      // Sum up all transactions run
      uint64_t total_txn_cnt = 0;
      for (uint64_t& cnt : txn_cnts)
        total_txn_cnt += cnt;
      return total_txn_cnt;
    }

  public:

    static int execute(int argc, const char* const* argv) {
      // TODO: figure out what this value means.  I think this value tells the
      // profiler to spawn perf on perf record. This might be useful in the future
      // to check where the performance bottlenecks are.
      bool spawn_perf = false;

      long num_items = NUM_ITEMS;
      int num_threads = DEFAULT_NUM_THREADS;
      int num_warehouses = DEFAULT_NUM_WAREHOUSES;
      trans_type trans = DEFAULT_TRANS;

      // TODO: should I have a time limit be a variable?
      double time_limit = TIME_LIMIT;

      int ret = parse_and_set_vars(argc, argv, num_threads,
                                   num_warehouses, trans);
      if (ret != 0) {
        return ret;
      }
#ifdef PERF_DEBUG
      fprintf(stderr, "num_items is %lu\n", num_items);
      fprintf(stderr, "num_threads is %d\n", num_threads);
      fprintf(stderr, "num_warehouses is %d\n", num_warehouses);
      fprintf(stderr, "transaction type (0 is default, 1 is two-phase transaction) is %d\n", trans);
#endif

      // TODO: make sure that you check for nonzero values in varaibles here!

      auto cpug_freq = determine_cpu_freq();
      if (cpu_freq == 0.0)
        return 1;
      else
        db_params::constants::processor_tsc_frequency = cpu_freq;

      // Initialization
      bench::db_profiler prof(spawn_perf);
      std::vector<concurrent_warehouse_type> warehouses =
        std::vector<concurrent_warehouse_type>();

      // Begin pre-population of warehouse list and warehouses
      prepopulate_database(num_items, num_warehouses, warehouses);

      // Run benchmark with time limit
      prof.start( Profiler::perf_mode::record);
      uint64_t total_txn_count = run_benchmark(warehouses, prof, num_threads,
                                               time_limit, trans);
      prof.finish(total_txn_count);
      return 0;
    }
};

static inline void print_usage(const char *argv_0) {
    std::stringstream ss;
    ss << "Usage of " << std::string(argv_0) << ":" << std::endl
       << "  --nthreads=<NUM> (or -h<NUM>)" << std::endl
       << "    Specify the number of threads to run transactions in the experiment" << std::endl
       << "  --nwarehouses=<NUM> (or -w<NUM>)" << std::endl
       << "    Specify the number of \"warehouses\", or partitions to the database" << std::endl
       << "  --trans_type=<STRING> (or -t<STRING>)" << std::endl
       << "    Specify the type of transaction to run, which is either a two phase transaction "<< std::endl
      << "(selected with \"2pt\" or \"two_phase\") or the default STO transaction " << std::endl
      << "(selected with \"default\")" << std::endl;
    std::cout << ss.str() << std::flush;
}

double db_params::constants::processor_tsc_frequency;

int main(int argc, const char* const* argv) {
  int ret_code = 
    concurrent_warehouse_experiment<db_params::db_default_params>::execute(argc, argv);
  return ret_code;
}
