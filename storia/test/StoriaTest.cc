#include <iostream>
#include <sstream>
#include <thread>

#include "Storia.hh"
#include "TestUtil.hh"

// Test suites
#include "dataflow/Tests.hh"
#include "integration/Tests.hh"

namespace storia {

namespace test {

enum {
    opt_cc = 1, opt_inte, opt_nrdrs, opt_nwtrs, opt_unit
};

enum ConcurrencyControlOptions {
    OCC = 1, Opaque, MVCC
};

static const Clp_Option options[] = {
    { "cc",         'c', opt_cc,    Clp_ValString, Clp_Optional },
    { "integration",'i', opt_inte,  Clp_NoVal,     Clp_Negate | Clp_Optional }, 
    { "readers",    'r', opt_nrdrs, Clp_ValInt,    Clp_Optional },
    { "unit",       'u', opt_unit,  Clp_NoVal,     Clp_Negate | Clp_Optional }, 
    { "writers",    'w', opt_nwtrs, Clp_ValInt,    Clp_Optional },
};

static inline void print_usage(const char *argv_0) {
    std::stringstream ss;
    ss  << "Usage of " << std::string(argv_0) << ":" << std::endl
        << "  --cc=<STRING> (or -c<STRING>)" << std::endl
        << "    Specify the type of concurrency control used. "
        <<     "Can be one of the followings:" << std::endl
        << "      default, opaque, mvcc" << std::endl
        << "  --integration (or -i)" << std::endl
        << "    Run integration tests (ignores -r and -w)." << std::endl
        << "  --readers=<NUM> (or -r<NUM>)" << std::endl
        << "    Specify the number of reader threads (default 1)." << std::endl
        << "  --unit (or -u)" << std::endl
        << "    Run unit tests (ignores -r and -w)." << std::endl
        << "  --writers=<NUM> (or -w<NUM>)" << std::endl
        << "    Specify the number of writer threads (default 0)." << std::endl;
    std::cout << ss.str() << std::flush;
}

};  // namespace test
};  // namespace storia

using namespace storia::test;

int main(int argc, const char* const argv[]) {
    Sto::global_init();

    size_t num_readers = 0;
    size_t num_writers = 0;
    bool run_integration = false;
    bool run_unit = false;

    {
        Clp_Parser *clp = Clp_NewParser(argc, argv, arraysize(options), options);
        int opt;
        int ret = 0;
        bool clp_stop = false;
        while (!clp_stop && ((opt = Clp_Next(clp)) != Clp_Done)) {
            switch (opt) {
                case opt_cc:
                    break;
                case opt_nrdrs:
                    num_readers = clp->val.i;
                    break;
                case opt_nwtrs:
                    num_writers = clp->val.i;
                    break;
                case opt_inte:
                    run_integration = !clp->negated;
                    break;
                case opt_unit:
                    run_unit = !clp->negated;
                    break;
                default:
                    print_usage(argv[0]);
                    clp_stop = true;
                    ret = 1;
                    break;
            }
        }
        Clp_DeleteParser(clp);

        if (ret) {
            return ret;
        }
    }

    size_t num_threads = num_readers + num_writers;
    (void)num_threads;

    bool run_toy = true;
    if (run_unit) {
        run_toy = false;
        logics::testMain();
        operators::testMain();
        state::testMain();
    }

    if (run_integration) {
        run_toy = false;
        filtercount::testMain();
    }
    
    if (run_toy) {
        std::cout << "Readers: " << num_readers << std::endl;
        std::cout << "Writers: " << num_writers << std::endl;
    }
}
