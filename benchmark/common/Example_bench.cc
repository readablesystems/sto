// The structure of this file is:
// 1. Common headers (including common/Common_bench.hh)
// 2. Data structures
// 3. Command line parameters
// 4. Database setup
// 5. Workload setup
// 6. Benchmark execution
// These sections can be each searched for verbatim

// Common headers
#include "common/Common_bench.hh"

// Data structures headers, as needed
#include "Example_structs_generated.hh"

// Declare a namespace -- it's good practice for code isolation!
namespace example_bench {

//
// Data structures
//

//using ordered_key = db_common::keys::single_key<int64_t>;
using ordered_key = db_common::keys::double_key<int64_t>;  // Two-part key
using unordered_key = db_common::keys::single_key<int64_t>;
//using unordered_key = db_common::keys::double_key<int64_t>;  // Two-part key

//
// Command line parameters
//

// This is the structure for holding additional command line parameters. Make
// sure to pass it in as the template argument to the parser class!
struct CommandParams : db_common::ClpCommandParams {
    explicit CommandParams() : db_common::ClpCommandParams() {}

    void print() override {
        db_common::ClpCommandParams::print();
        std::cout << "Additional param: " << "some setting" << std::endl
                  ;
    }

    // Members
};

//
// Database Setup
//

// This is the database type. It defines the tables, too.
template <typename DBParams>
class Example_db : public db_common::common_db<DBParams> {
public:
    using base_db = db_common::common_db<DBParams>;
    template <typename K, typename V>
    using OIndex = typename base_db::template OIndex<K, V>;
    template <typename K, typename V>
    using UIndex = typename base_db::template UIndex<K, V>;

    using base_db::rng;

    // Can also be passed in as a command line parameter!
    static constexpr size_t table_size = 1000000;

    template <typename CommandParams>
    Example_db(CommandParams& params)
        : base_db(params), params(params),
          tbl_ordered(table_size), tbl_unordered(table_size) {}

    template <typename CommandParams, typename RunParams>
    void setup(CommandParams&, RunParams&) {
        // Prepopulation goes in here
        static uint64_t nthreads = topo_info.num_cpus;
        static ssize_t block_size = table_size / nthreads;

        std::vector<int64_t> prepop_count(nthreads, 0);

        base_db::prepopulate([&] (uint64_t thread_id) {
            // Choose a distribution
            // ex: Prepopulate with a random value
            auto dist = sampling::StoUniformDistribution<>(rng[thread_id], 0, table_size - 1);

            // Set the bounds for each thread
            ssize_t start_id = thread_id * block_size;
            ssize_t end_id = (thread_id + 1 < nthreads ?
                (thread_id + 1) * block_size : table_size) - 1;

            // Prepopulate with data
            for (int32_t id = start_id; id <= end_id; ++id) {
                int32_t payload = dist.sample_idx();
                data_value value = { .payload = payload, .version = 0 };
                tbl_ordered.nontrans_put(ordered_key(id, id), value);
                tbl_unordered.nontrans_put(ordered_key(id), value);
                prepop_count[thread_id] += payload;
            }
        });

        prepopulate_data.count = 0;
        for (auto count : prepop_count) {
            prepopulate_data.count += count;
        }
    }

    void thread_init_all() override {
        base_db::thread_init(tbl_ordered, tbl_unordered);
    }

    const CommandParams params;
    struct {
        int64_t count;
    } prepopulate_data;

    OIndex<ordered_key, ordered_value> tbl_ordered;
    UIndex<unordered_key, unordered_value> tbl_unordered;
};
// This is an alias for the DB type. Or we could've just called it db_type to
// begin with.
template <typename DBParams> using db_type = Example_db<DBParams>;

//
// Workload setup
//

// This is the transaction runner class. It must implement run_txn(), which is
// called for each loop-iteration. In run_txn(), the workload must be generated,
// executed, and either aborted/retried if necessary. The return value is the
// number of successful operations (this is typically 1 for retry-loop workloads
// and successful no-retry workloads, and 0 for failed no-retry workloads).
template <typename DBParams>
class Example_runner : public db_common::common_runner<db_type<DBParams>> {
public:
    using base_runner = db_common::common_runner<db_type<DBParams>>;

    using base_runner::db;
    template <typename T>
    using Record = typename base_runner::template Record<T>;

    template <typename... Args>
    Example_runner(Args&&... args)
        : base_runner(std::forward<Args>(args)...),
          txn_gen(db.rng[0], 0, 99) {}

    // For convenience
    enum txn_type {
        read = 0
    };

    // Generate a single transaction to run
    inline txn_type generate_transaction() {
        return read;
    }

    // Implement this!
    size_t run_txn() override {
        switch (generate_transaction()) {
        case read:
        {
            return 1;
        }
        default:
        {
            break;
        }
        }
        return 0;
    }

    // Transaction implementations

    sampling::StoUniformDistribution<> txn_gen;
};
// This is an alias for the runner type. Or we could've just called it
// runner_type to begin with.
template <typename DBParams> using runner_type = Example_runner<DBParams>;

//
// Benchmark execution
//

// This is just to make the run() function a bit cleaner.
template <typename DBParams>
using bench_type = db_common::bench_access<DBParams, db_type<DBParams>, runner_type<DBParams>>;

// Where main code execution begins. We do this here instead of in main() for
// namespace scoping reasons.
int run(int argc, const char * const *argv) {
    // Create the command line arguments parser wrapper; add custom CommandParams
    // types as a template parameter.
    db_common::ClpParser<CommandParams> parser(argc, argv);

    // Add additional command line options. Collisions will be detected at run
    // time and will cause the entire program to abort. An example option is:
    /*
    parser.AddOption(
            "myoption", 'm', Clp_ValString, Clp_Optional,
            [] (CommandParams& params, std::any value) -> bool {
                // Cast the value back from std::any
                const char* str = std::any_cast<const char*>(value);
                // Return true if the value is acceptable; false otherwise.
                return strlen(str) > 0;
            },
            "Here is an example option. Can be one of:",
            "  any non-empty string.");
    */

    // Go!
    return db_common::template run<bench_type>(parser);
}

};  // namespace example_bench

int main(int argc, const char * const *argv) {
    return example_bench::run(argc, argv);
}
