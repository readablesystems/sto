H#include <iostream>
#include <stdlib.h>
#include <string>
#include <unordered_set>

#include "clp.h"
#include "PlatformFeatures.hh"
#include "Transaction.hh"
#include "DB_index.hh"
#include "DB_params.hh"
#include "DB_profiler.hh"

#define MIN_KEY_LENGTH 10
#define MAX_KEY_LENGTH 20

// Change this value to change the number of iterations run on the same
// transaction.
#define NUM_ITERATIONS 100

// The lowest and highest value strings. We know the highest value string is all
// z's because 'z' is the highest value returned by 'random_a_string'.
static const std::string LOWEST_STRING = "";
static const std::string GREATEST_STRING = std::string(MAX_KEY_LENGTH, 'z');

enum trans_type {TWO_PHASE, DEFAULT};

enum color {RED, BLUE};

struct row {
  enum class NamedColumn : int {name = 0};
  color value;

  row(): value() {}
  row(color val): value(val) {}
};

typedef std::string item_key;
typedef row color_row;

// clp parser definitions
enum {
    opt_nitems = 1, opt_pblue, opt_ttrans
};

static const Clp_Option options[] = {
    { "nitems",  'n', opt_nitems,  Clp_ValInt, Clp_Optional },
    { "pblue",  'p', opt_pblue,  Clp_ValInt, Clp_Optional },
    { "trans_type", 't', opt_ttrans, Clp_ValString, Clp_Optional }
};

static inline void print_usage(const char *argv_0);

// Psuedo-random number generator
#ifdef PERF_DEBUG
static std::mt19937 gen(0);
#else
static std::mt19937 gen(time(NULL));
#endif

// Class implementing the blue-red experiment
template <typename DBParams>
class blue_red_experiment {
  typedef bench::ordered_index<item_key, color_row, DBParams> blue_red_db_type;

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

  static void insert_random_item(blue_red_db_type& db, bool blue) {
    std::string rand_key = random_a_string(MIN_KEY_LENGTH, MAX_KEY_LENGTH);
    color_row value = (blue ? color_row(BLUE) : color_row(RED));
    db.nontrans_put(rand_key, value);
  }
  
public:
  static void prepopulate_db(blue_red_db_type& db, uint64_t num_items,
                             int percent_blue) {
    // Set the seed of rand
#ifdef PERF_DEBUG
    srand(1);
#else    
    srand(time(NULL));
#endif
    
    uint64_t nblue_items = (num_items * (percent_blue / 100.0));
    uint64_t nred_items = num_items - nblue_items;
    
#ifdef PERF_DEBUG
    printf("nblue_items is %lu\n", nblue_items);
    printf("nred_items is %lu\n", nred_items);
#endif

    // If all items of one color have already been inserted, just insert the
    // other color. Otherwise, let there be a 50-50 chance of inserting an item
    // with the blue color.
    while(nblue_items != 0 || nred_items != 0) {
      bool blue;
      
      if (nblue_items == 0) {
        blue = false;
      }
      else if (nred_items == 0) {
        blue = true;
      }
      else {
        blue = (rand() % 2) == 0;
      }

      if (blue) {
        insert_random_item(db, true);
        nblue_items--;
      }
      else {
        insert_random_item(db, false);
        nred_items--;
      }
    }

#ifdef PERF_DEBUG
    printf("Finished inserting items.\n");
    printf("nblue_items is %lu\n", nblue_items);
    printf("nred_items is %lu\n", nred_items);
#endif

  }

  static void run_default_benchmark(blue_red_db_type& db, 
                                    bench::db_profiler& prof) {
    {
      TransactionGuard t;
      
      // Initialize set that will store keys of items with the red property.
      std::unordered_set<std::string> blue_keys;

      // Create a closure that will allow us to reference and modify
      // 'blue_keys'. This function will only record items with the blue property.
      auto filter_function =
        [&] (const item_key& key, const color_row& val) {
          if(val.value == BLUE) {
            blue_keys.insert(key);
          }
          return true;
        };

      bench::RowAccess default_access = bench::RowAccess::None;

      // Call the range query with the filter function.
      bool success = db.template range_scan
        <decltype(filter_function), false>
        (LOWEST_STRING, GREATEST_STRING, filter_function, default_access);
      assert(success);
      
      bench::RowAccess update_access = bench::RowAccess::UpdateValue;
      // The experiment is to turn all blue keys red. However, we insert a new
      // blue key because we want to be able to run the experiment many times.
      color_row new_row(BLUE);

      // Update all keys leading to items to have the 'red' property instead.
      for (auto blue_key : blue_keys) {
        bool success, result;
        uintptr_t row;
        const void* value;

        std::tie(success, result, row, value) =
          db.select_row(blue_key, update_access);
        db.update_row(row, &new_row);
      }

      // fprintf(stderr, "$ Current size of tset: %u\n", Sto::get_tset_size());
    }
  }
  
  static void run_two_phase_benchmark(blue_red_db_type& db, uint64_t num_items,
                                      bench::db_profiler& prof) {
    (void)num_items;
    {
      TransactionGuard t;
      Sto::start_two_phase_transaction();
      
      // Initialize set that will store keys of items with the blue property.
      std::unordered_set<std::string> blue_keys;

      // Create a closure that will allow us to reference and modify
      // 'blue_keys'. This function will only record items with the blue property.
      auto filter_function =
        [&] (const item_key& key, const color_row& val, const int subaction_id) {
          if(val.value == BLUE) {
            blue_keys.insert(key);
          } else {
            Sto::abort_subaction(subaction_id);
          }
          return true;
        };

      bench::RowAccess default_access = bench::RowAccess::None;

      // Call the range query with the filter function.
      bool success = db.template range_scan_subaction
        <decltype(filter_function), false>
        (LOWEST_STRING, GREATEST_STRING, filter_function, default_access);
      assert(success);
      
      Sto::phase_two();

      bench::RowAccess update_access = bench::RowAccess::UpdateValue;
      // The experiment is to turn all blue keys red. However, we insert a new
      // blue key because we want to be able to run the experiment many times.
      color_row new_row(BLUE);

      // Update all keys leading to items to have the blue property instead.
      for (auto blue_key : blue_keys) {
        bool success, result;
        uintptr_t row;
        const void* value;

        std::tie(success, result, row, value) =
          db.select_row(blue_key, update_access);
        db.update_row(row, &new_row);
      }

      // fprintf(stderr, "$ Current size of tset: %u\n", Sto::get_tset_size());
    }
  }
    
  static void run_benchmark(blue_red_db_type& db, uint64_t num_items,
                            trans_type t_trans, bench::db_profiler& prof) {
    switch(t_trans) {
      case TWO_PHASE:
        run_two_phase_benchmark(db, num_items, prof);
        break;
      case DEFAULT:
        run_default_benchmark(db, prof);
        break;
      default:
        printf("Transaction type not known!\n");
        break;
    };    
  }


  static int parse_and_set_vars(int argc, const char* const* argv, 
                         uint64_t& num_items, int& percent_blue,
                                trans_type& t_trans) {
    Clp_Parser* clp = Clp_NewParser(argc, argv, arraysize(options), options);
    int opt;
    int ret = 0;
    bool clp_stop = false;
    while (!clp_stop && ((opt = Clp_Next(clp)) != Clp_Done)) {
      switch (opt) {
      case opt_nitems:
        num_items = clp->val.i;
        break;
      case opt_pblue:
        percent_blue = clp->val.i;
        break;
      case opt_ttrans:
        if (strcmp(clp->val.s, "2pt") == 0 || strcmp(clp->val.s, "two_phase") == 0) {
          t_trans = TWO_PHASE;
        }
        else if (strcmp(clp->val.s, "default") == 0) {
          t_trans = DEFAULT;
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
  
  static int execute(int argc, const char* const* argv) {
    bool spawn_perf = false;

    uint64_t num_items = 1000;
    int percent_blue = 50;
    trans_type t_trans = TWO_PHASE;

    int ret = parse_and_set_vars(argc, argv, num_items, percent_blue, t_trans);
    if (ret != 0) {
      return ret;
    }

#ifdef PERF_DEBUG
    printf("num_items is %lu\n", num_items);
    printf("percent_blue is %d\n", percent_blue);
#endif
    
    if (num_items < 1) {
      // We don't want to experiment with less than one item.
      return 1;
    }

    if (percent_blue < 0 || percent_blue > 100) {
      // Out of range percent_blue.
      return 1;
    }

    auto cpu_freq = determine_cpu_freq();
    if (cpu_freq == 0.0) {
      return 1;
    } else {
      db_params::constants::processor_tsc_frequency = cpu_freq;
    }
  
    bench::db_profiler prof(spawn_perf);
    blue_red_db_type db(num_items);

    std::cout << "Prepopulating database..." << std::endl;
    prepopulate_db(db, num_items, percent_blue);
    std::cout << "Prepopulation complete." << std::endl;
    std::cout << "Running Benchmark 100 times" << std::endl;
    prof.start(Profiler::perf_mode::record);
    for (int i = 0; i < NUM_ITERATIONS; i++) {
      run_benchmark(db, num_items, t_trans, prof);
    }
    prof.finish(100); // We're only running 100 transactions in this experiment.
    std::cout << "Benchmark Complete" << std::endl;

    return 0;
  }
};

static inline void print_usage(const char *argv_0) {
    std::stringstream ss;
    ss << "Usage of " << std::string(argv_0) << ":" << std::endl
       << "  --nitems=<NUM> (or -n<NUM>)" << std::endl
       << "    Specify the number of items present in the database (default DEFAULT)." << std::endl
       << "  --pblue=<NUM> (or -p<NUM>)" << std::endl
       << "    Specify the percentage (as an integer) of items in the database with the blue property (default DEFAULT)." << std::endl
       << "  --trans_type=<STRING> (or -t<STRING>)" << std::endl
       << "    Specify the type of transaction to run, which is either a two phase transaction (selected with \"2pt\" or \"two_phase\") or the default STO transaction (selected with \"default\")" << std::endl;
    std::cout << ss.str() << std::flush;
}

double db_params::constants::processor_tsc_frequency;

int main(int argc, const char* const* argv) {
  int ret_code = 
    blue_red_experiment<db_params::db_default_params>::execute(argc, argv);
  
  return ret_code;
}
