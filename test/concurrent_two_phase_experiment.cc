#include <iostream>

#include "clp.h"
#include "PlatformFeatures.hh"
#include "Transaction.hh"
#include "DB_index.hh"
#include "DB_params.hh"
#include "DB_profiler.hh"

enum trans_type { DEFAULT, TWO_PHASE };

enum color {RED, BLUE};

struct row {
  enum class NamedColumn : int {name = 0};
  color value;

  row(): value() {}
  row(color val): value(val) {}
};

typedef std::string item_key;
typedef row color_row;

#define DEFAULT_NUM_ITEMS 10000
#define DEFAULT_PERCENT_BLUE 50
#define DEFAULT_NUM_THREADS 3
#define DEFAULT_NUM_WAREHOUSES 3
#define DEFAULT_TRANS TWO_PHASE

// clp parser definitions
enum {
    opt_nitems = 1, opt_pblue, opt_nthreads, opt_nwarehouses, opt_ttrans
};

static const Clp_Option options[] = {
    { "nitems",  'n', opt_nitems,  Clp_ValInt, Clp_Optional },
    { "pblue",  'p', opt_pblue,  Clp_ValInt, Clp_Optional },
    { "nthreads", 'h', opt_nthreads, Clp_ValInt, Clp_Optional},
    { "nwarehouses", 'w', opt_nwarehouses, Clp_ValInt, Clp_Optional},
    { "trans_type", 't', opt_ttrans, Clp_ValString, Clp_Optional }
};

static inline void print_usage(const char *argv_0);

template <typename DBParams>
class concurrent_blue_red_warehouse_experiment {
  typedef bench::ordered_index<item_key, color_row, DBParams> concurrent_blue_red_warehouse_type;

  static int parse_and_set_vars(int argc, const char* const* argv,
                                uint64_t& num_items, int& percent_blue,
                                int& num_threads, int& num_warehouses,
                                trans_type& trans) {
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

public:
  static int execute(int argc, const char* const* argv) {
    // TODO: figure out what this value means.  I think this value tells the
    // profiler to spawn perf on perf record. This might be useful in the future
    // to check where the performance bottlenecks are. 
    bool spawn_perf = false;

    uint64_t num_items = DEFAULT_NUM_ITEMS;
    int percent_blue = DEFAULT_PERCENT_BLUE;
    int num_threads = DEFAULT_NUM_THREADS;
    int num_warehouses = DEFAULT_NUM_WAREHOUSES;
    trans_type trans = DEFAULT_TRANS;
    
    int ret = parse_and_set_vars(argc, argv, num_items, percent_blue, num_threads,
                                 num_warehouses, trans);
    if (ret != 0) {
      return ret;
    }
#ifdef PERF_DEBUG
    fprintf(stderr, "num_items is %lu\n", num_items);
    fprintf(stderr, "percent_blue is %d\n", percent_blue);
    fprintf(stderr, "num_threads is %d\n", num_threads);
    fprintf(stderr, "num_warehouses is %d\n", num_warehouses);
    fprintf(stderr, "transaction type (0 is default, 1 is two-phase transaction) is %d\n", trans);
#endif
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
       << "  --nthreads=<NUM> (or -h<NUM>)" << std::endl
       << "    Specify the number of threads to run transactions in the experiment" << std::endl
       << "  --nwarehouses=<NUM> (or -w<NUM>)" << std::endl
       << "    Specify the number of \"warehouses\", or partitions to the database" << std::endl
       << "  --trans_type=<STRING> (or -t<STRING>)" << std::endl
       << "    Specify the type of transaction to run, which is either a two phase transaction (selected with \"2pt\" or \"two_phase\") or the default STO transaction (selected with \"default\")" << std::endl;
    std::cout << ss.str() << std::flush;
}

int main(int argc, const char* const* argv) {
  int ret_code = 
    concurrent_blue_red_warehouse_experiment<db_params::db_default_params>::execute(argc, argv);
  return ret_code;
}
