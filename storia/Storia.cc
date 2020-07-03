#include <iostream>
#include <sstream>
#include <thread>

#include "Storia.hh"

namespace storia {

enum {
    opt_cc = 1, opt_nrdrs, opt_nwtrs
};

enum ConcurrencyControlOptions {
    OCC = 1, Opaque, MVCC
};

static const Clp_Option options[] = {
    { "cc",           'c', opt_cc,    Clp_ValString, Clp_Optional },
    { "readers",      'r', opt_nrdrs, Clp_ValInt,    Clp_Optional },
    { "writers",      'w', opt_nwtrs, Clp_ValInt,    Clp_Optional },
};

static inline void print_usage(const char *argv_0) {
    std::stringstream ss;
    ss  << "Usage of " << std::string(argv_0) << ":" << std::endl
        << "  --cc=<STRING> (or -c<STRING>)" << std::endl
        << "    Specify the type of concurrency control used. "
        <<     "Can be one of the followings:" << std::endl
        << "      default, opaque, mvcc" << std::endl
        << "  --readers=<NUM> (or -r<NUM>)" << std::endl
        << "    Specify the number of reader threads (default 1)." << std::endl
        << "  --writers=<NUM> (or -w<NUM>)" << std::endl
        << "    Specify the number of writer threads (default 0)." << std::endl;
    std::cout << ss.str() << std::flush;
}

};  // namespace storia

using namespace storia;

int main(int argc, const char* const argv[]) {
    Sto::global_init();

    size_t num_readers = 0;
    size_t num_writers = 0;

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
                default:
                    print_usage(argv[0]);
                    clp_stop = true;
                    ret = 1;
                    break;
            }
            Clp_DeleteParser(clp);
        }

        if (ret) {
            return ret;
        }
    }

    size_t num_threads = num_readers + num_writers;

    std::cout << "Readers: " << num_readers << std::endl;
    std::cout << "Writers: " << num_writers << std::endl;

    {
        auto pred = PredicateUtil::Make<int, double>(
            [] (const int& x, const double& y) -> bool {
                return x > y;
            }, 0.f);

        std::cout << "Pred (1): " << pred.eval(1) << std::endl;
        std::cout << "Pred (-1): " << pred.eval(-1) << std::endl;
    }
}
