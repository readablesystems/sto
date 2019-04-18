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
#include <unordered_set>

#ifndef SHORT_CONCURRENT_TWO_PHASE_EXPERIMENT
// Size of each index in the experiment
#define NUM_ITEMS 10000
#define DEFAULT_NUM_THREADS 4
#define DEFAULT_NUM_WAREHOUSES 4
#define TIME_LIMIT 60
// Number of items generated in each transaction to order
#define NUM_ITEMS_IN_ORDER 500
#else
#define NUM_ITEMS 1000
#define DEFAULT_NUM_THREADS 2
#define DEFAULT_NUM_WAREHOUSES 2
#define TIME_LIMIT 1
#define NUM_ITEMS_IN_ORDER 100
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
  using concurrent_warehouse_type =
    bench::unordered_index<item_key, item_value, DBParams>;
  using warehouse_list_type = std::vector<concurrent_warehouse_type>;
  static const bench::RowAccess default_access = bench::RowAccess::ObserveValue;
  static const bench::RowAccess read_for_update_access =
    bench::RowAccess::UpdateValue;

  struct lookup_subaction_ret {
    bool success;
    bool found;
    uintptr_t row_ptr;
    const item_value* value;
    int subaction_id;

    lookup_subaction_ret(std::tuple<bool, bool, uintptr_t,
                         const item_value*, int> sel_return) {
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
        if (strcmp(clp->val.s, "2pt") == 0 ||
            strcmp(clp->val.s, "two_phase") == 0) {
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

  // Reused from TPCC_bench.hh
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

  static int generate_warehouse_business_metric() {
    return random_dist(0, 50);
  }

  static int generate_warehouse_distance_metric() {
    return random_dist(0, 50);
  }

  static void insert_random_item(concurrent_warehouse_type& index,
                                 item_key key) {
    item_value value = item_value(random_dist(0, 100), random_dist(0, 100));
    index.nontrans_put(key, value);
  }

  static void prepopulate_database(long num_items, unsigned int num_warehouses,
                                   warehouse_list_type& warehouses) {
    for (unsigned i = 0; i < num_warehouses; i++) {
      concurrent_warehouse_type* warehouse =
        new concurrent_warehouse_type(num_items);
      // Insert random items into each 'warehouse'
      for (long j = 0; j < num_items; j++) {
        insert_random_item(*warehouse, std::to_string(j));
      }
      warehouses.push_back(*warehouse);
    }
  }

#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
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
#endif

  static void run_two_phase_transaction(warehouse_list_type& warehouses) {
    TRANSACTION {
      Sto::start_two_phase_transaction();

#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
      debug_print("Two phase transaction starting");
#endif

      // Select items to order by randomly generating keys.
      std::unordered_set<std::string> order_keys;
      for (int i = 0; i < NUM_ITEMS_IN_ORDER; i++) {
#ifndef CONTROLLED_EXPERIMENT_TWO_PHASE
        order_keys.insert(std::to_string(random_dist(0, NUM_ITEMS - 1)));
#else
        order_keys.insert(std::to_string(i));
#endif
      }

#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
      debug_print("order keys size is " + std::to_string(order_keys.size()));
#endif


      // Get metrics for each warehouse
      int best_warehouse_index = -1;
      int best_score = std::numeric_limits<int>::min();
      int best_subaction_id = -1;

      for (int i = 0; i < warehouses.size(); i++) {
        int curr_score = 0;
        int curr_subaction_id = -1;

        bool first_element = true;
        // Metric for amount of items left in supply
        for (auto it = order_keys.begin(); it != order_keys.end(); it++) {
          lookup_ret ret;
          if (first_element) {
            first_element = false;

            // Create a subaction for the first element
            lookup_subaction_ret sret =
              lookup_subaction_ret(warehouses[i].select_row_subaction(*it));

            curr_subaction_id = sret.subaction_id;
            ret =
              lookup_ret(std::tuple<bool, bool, uintptr_t, const item_value*>
                         (sret.success, sret.found, sret.row_ptr, sret.value));
          }
          else {
            ret = lookup_ret(warehouses[i].select_row(*it));
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
        int warehouse_busy_metric = generate_warehouse_business_metric();
        curr_score -= warehouse_busy_metric;

        // Metric for distance of warehouse
        int warehouse_distance_metric = generate_warehouse_distance_metric();
        curr_score -= warehouse_distance_metric;

        // Skip logic to select best warehouse if flag is on, which will have
        // each thread process transactions in different warehouses.
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
            Sto::abort_subaction(best_subaction_id);
          }
          best_subaction_id = curr_subaction_id;
          best_warehouse_index = i;

#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
          debug_print("current score is larger than best score.");
          debug_print("new best score is " + std::to_string(best_score) +
                      " with subaction id " + std::to_string(best_subaction_id) +
                      " and warehouse index " +
                      std::to_string(best_warehouse_index));
#endif
        }
        else {
          Sto::abort_subaction(curr_subaction_id);
        }
#else
        // Threads run on different warehouses. 2PT shouldn't abort on commit.
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

      Sto::phase_two();

#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
      if (Sto::num_active_subactions() != 1) {
        debug_print("Expected one subaction running, found " +
                    std::to_string(Sto::num_active_subactions()));
      }
      always_assert(Sto::num_active_subactions() == 1);
#endif

      // Read the value of each item in the warehouse we chose. Then update item.
      for (auto it = order_keys.begin(); it != order_keys.end(); it++) {
        lookup_ret ret =
          lookup_ret(warehouses[best_warehouse_index].select_row(*it, true));
        TXN_DO(ret.success);
#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
        always_assert(ret.found && "item not found or aborted");
#endif
        item_value new_value = { ret.value->quantity,
                                 ret.value->ordered + 1 };
        warehouses[best_warehouse_index].update_row(ret.row_ptr, &new_value);
      }

      // TODO: write more items in the second phase.

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
      std::unordered_set<std::string> order_keys;
      for (int i = 0; i < NUM_ITEMS_IN_ORDER; i++) {
#ifndef CONTROLLED_EXPERIMENT_TWO_PHASE
        order_keys.insert(std::to_string(random_dist(0, NUM_ITEMS - 1)));
#else
        order_keys.insert(std::to_string(i));
#endif
      }

#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
      debug_print("order keys size is " + std::to_string(order_keys.size()));
#endif

      // Get metrics for each warehouse
      int best_warehouse_index = -1;
      int best_score = std::numeric_limits<int>::min();

      for (int i = 0; i < warehouses.size(); i++) {
        int curr_score = 0;

        // Metric for amount of items left in supply.
        for (auto it = order_keys.begin(); it != order_keys.end(); it++) {
          lookup_ret ret =
            lookup_ret(warehouses[i].select_row(*it));
          // If the lookup doesn't succeed, abort the transaction
          TXN_DO(ret.success);
#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
          always_assert(ret.found &&
                        "item not found or aborted. Thread conflict?");
#endif
          int warehouse_supply_metric = ret.value->quantity - ret.value->ordered;
          curr_score += warehouse_supply_metric;
        }

        // Metric for business of warehouse.
        int warehouse_busy_metric = generate_warehouse_business_metric();
        curr_score -= warehouse_busy_metric;

        // Metric for distance of warehouse.
        int warehouse_distance_metric = generate_warehouse_distance_metric();
        curr_score -= warehouse_distance_metric;

        // Skip logic to select best warehouse if flag is on, which will have
        // each thread process transactions in different warehouses.
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
                      " with warehouse index " +
                      std::to_string(best_warehouse_index));
#endif
        }
#else
        // Each thread runs on a different warehouse. Should have some aborts at
        // commit time.
        int runner_id = ::TThread::id();
        if (i == runner_id) {
          best_warehouse_index = i;
        }
#endif
      }

      // Read the value of each item in the warehouse we chose.  Then update the
      // item, incrementing the amount ordered by 1.
      for (auto it = order_keys.begin(); it != order_keys.end(); it++) {
        lookup_ret ret =
          lookup_ret(warehouses[best_warehouse_index].select_row(*it, true));
        TXN_DO(ret.success);
#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
        always_assert(ret.found && "item not found or aborted");
#endif
        item_value new_value = { ret.value->quantity,
                                 ret.value->ordered + 1 };
        warehouses[best_warehouse_index].update_row(ret.row_ptr, &new_value);
      }

      // TODO: write more items in the second phase.

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
                                  void (*transaction_method)
                                  (warehouse_list_type&)) {
      uint64_t local_txn_cnt = 0;
      ::TThread::set_id(runner_id);

      // TODO: uncomment when running on evaluation machine.
      // set_affinity(runner_id);

      uint64_t tsc_diff =
        (uint64_t)(time_limit * db_params::constants::processor_tsc_frequency *
                   db_params::constants::billion);
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
                             std::ref(prof), i, time_limit,
                             std::ref(txn_cnts[i]), transaction_method);
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
      bool spawn_perf = false;

      long num_items = NUM_ITEMS;
      int num_threads = DEFAULT_NUM_THREADS;
      int num_warehouses = DEFAULT_NUM_WAREHOUSES;
      trans_type trans = DEFAULT_TRANS;
      double time_limit = TIME_LIMIT;

      int ret = parse_and_set_vars(argc, argv, num_threads,
                                   num_warehouses, trans);
      if (ret != 0) {
        return ret;
      }
#ifdef DEBUG_CONCURRENT_TWO_PHASE_EXPERIMENT
      fprintf(stderr, "num_items is %lu\n", num_items);
      fprintf(stderr, "num_threads is %d\n", num_threads);
      fprintf(stderr, "num_warehouses is %d\n", num_warehouses);
      fprintf(stderr, "txn type (0 is default, 1 is 2pt) is %d\n", trans);
#endif

      // TODO: Check for nonzero values in variables here

      auto cpu_freq = determine_cpu_freq();
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
       << "    Specify the number of threads to run transactions " <<
      "in the experiment" << std::endl
       << "  --nwarehouses=<NUM> (or -w<NUM>)" << std::endl
       << "    Specify the number of \"warehouses\", or partitions to the " <<
      "database" << std::endl
       << "  --trans_type=<STRING> (or -t<STRING>)" << std::endl
       << "    Specify the type of transaction to run, which is either a" <<
      "two phase transaction "<< std::endl
      << "(selected with \"2pt\" or \"two_phase\") or the " <<
      " default STO transaction " << std::endl
      << "(selected with \"default\")" << std::endl;
    std::cout << ss.str() << std::flush;
}

double db_params::constants::processor_tsc_frequency;

int main(int argc, const char* const* argv) {
  int ret_code = 
    concurrent_warehouse_experiment
    <db_params::db_default_params>::execute(argc, argv);
  return ret_code;
}
