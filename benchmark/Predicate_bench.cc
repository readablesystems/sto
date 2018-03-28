#include "Predicate_bench.hh"
#include "DB_params.hh"

int main() {
    typedef db_params::db_default_params params;
    typedef predicate_bench::predicate_runner<params> runner_type;
    typedef predicate_bench::predicate_db<params> db_type;

    db_type db;
    initialize_db(db, 256);
    runner r(16/*nthreads*/, 10000000/*trans per thread*/, db);
    r.run();

    return 0;
}
