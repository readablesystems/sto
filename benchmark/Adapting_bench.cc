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

//
// Data structures
//

// Data structures headers, as needed
#include "Adapting_structs_generated.hh"

// Deferred update implementations
namespace commutators {

template <>
class Commutator<adapting_bench::adapting_value> {
private:
    enum op_type {
        write_heavy = 1, write_some, write_less
    };

public:
    Commutator() = default;

    explicit Commutator(int8_t operation) : operation(static_cast<op_type>(operation)) {}

    void operate(adapting_bench::adapting_value& value) const {
        switch (operation) {
        case write_heavy:
        {
            ++value.write_some;
            ++value.write_much;
            ++value.write_most;
            break;
        }
        case write_some:
        {
            ++value.write_much;
            ++value.write_most;
            break;
        }
        case write_less:
        {
            ++value.write_most;
            break;
        }
        }
    }

    using supports_cellsplit = bool;  // For detection purposes
    auto required_cells(int split) {
        using nc = adapting_bench::adapting_value::NamedColumn;
        switch (operation) {
        case write_heavy:
            return CommAdapter::required_cells<adapting_bench::adapting_value>(
                    {nc::write_some, nc::write_much, nc::write_most}, split);
        case write_some:
            return CommAdapter::required_cells<adapting_bench::adapting_value>(
                    {nc::write_much, nc::write_most}, split);
        case write_less:
            return CommAdapter::required_cells<adapting_bench::adapting_value>(
                    {nc::write_most}, split);
        }
        always_assert(false);
        return CommAdapter::required_cells<adapting_bench::adapting_value>(
                {}, split);
    }

    op_type operation;

private:
    friend Commutator<adapting_bench::adapting_value_cell0>;
    friend Commutator<adapting_bench::adapting_value_cell1>;
};

// And these are the implementations for STS
template <>
class Commutator<adapting_bench::adapting_value_cell0> : public Commutator<adapting_bench::adapting_value> {
public:
    template <typename... Args>
    Commutator(Args&&... args) : Commutator<adapting_bench::adapting_value>(std::forward<Args>(args)...) {}

    void operate(adapting_bench::adapting_value_cell0& value) const {
        switch (operation) {
        case write_heavy:
        {
            ++value.write_most;
            break;
        }
        case write_some:
        {
            ++value.write_most;
            break;
        }
        case write_less:
        {
            ++value.write_most;
            break;
        }
        }
    }
};

template <>
class Commutator<adapting_bench::adapting_value_cell1> : public Commutator<adapting_bench::adapting_value> {
public:
    template <typename... Args>
    Commutator(Args&&... args) : Commutator<adapting_bench::adapting_value>(std::forward<Args>(args)...) {}

    void operate(adapting_bench::adapting_value_cell1& value) const {
        switch (operation) {
        case write_heavy:
        {
            ++value.write_some;
            ++value.write_much;
            break;
        }
        case write_some:
        {
            ++value.write_much;
            break;
        }
        case write_less:
        {
            break;
        }
        }
    }
};

};

// Declare a namespace -- it's good practice for code isolation!
namespace adapting_bench {

//
// Data structures (cont.)
//

using adapting_key = db_common::keys::single_key<int64_t>;

//
// Command line parameters
//

// This is the structure for holding additional command line parameters. Make
// sure to pass it in as the template argument to the parser class!
struct CommandParams : db_common::ClpCommandParams {
    explicit CommandParams()
        : db_common::ClpCommandParams(),
          ops_per_txn(1000), table_size(1000000), variants(4) {}

    void print() override {
        db_common::ClpCommandParams::print();
        std::cout << "Table size                : " << table_size << std::endl
                  << "Operations per transaction: " << ops_per_txn << std::endl
                  << "Operation variants        : " << variants << std::endl
                  ;
    }

    // Members
    long ops_per_txn;
    long table_size;
    int32_t variants;
};

//
// Database Setup
//

// This is the database type. It defines the tables, too.
template <typename DBParams>
class Adapting_db : public db_common::common_db<DBParams> {
public:
    using base_db = db_common::common_db<DBParams>;
    template <typename K, typename V>
    using OIndex = typename base_db::template OIndex<K, V>;
    template <typename K, typename V>
    using UIndex = typename base_db::template UIndex<K, V>;

    using base_db::rng;

    template <typename CommandParams>
    Adapting_db(CommandParams& params)
        : base_db(params), params(params),
          tbl_adapting(params.table_size) {}

    template <typename CommandParams, typename RunParams>
    void setup(CommandParams& params, RunParams&) {
        // Prepopulation goes in here
        static uint64_t nthreads = topo_info.num_cpus;
        static ssize_t block_size = params.table_size / nthreads;

        std::vector<int64_t> prepop_count(nthreads, 0);

        base_db::prepopulate([&] (uint64_t thread_id) {
            // Choose a distribution
            // ex: Prepopulate with a random value
            auto dist = sampling::StoUniformDistribution<>(rng[thread_id], 0, 99);

            // Set the bounds for each thread
            ssize_t start_id = thread_id * block_size;
            ssize_t end_id = (thread_id + 1 < nthreads ?
                (thread_id + 1) * block_size : params.table_size) - 1;

            // Prepopulate with data
            for (int32_t id = start_id; id <= end_id; ++id) {
                int64_t v = dist.sample_idx();
                adapting_value value = { v, v, v, v };
                tbl_adapting.nontrans_put(adapting_key(id), value);
                prepop_count[thread_id] += v;
            }
        });

        prepopulate_data.count = 0;
        for (auto count : prepop_count) {
            prepopulate_data.count += count;
        }
    }

    void thread_init_all() override {
        base_db::thread_init(tbl_adapting);
    }

    const CommandParams& params;
    struct {
        int64_t count;
    } prepopulate_data;

    OIndex<adapting_key, adapting_value> tbl_adapting;
};
// This is an alias for the DB type. Or we could've just called it db_type to
// begin with.
template <typename DBParams> using db_type = Adapting_db<DBParams>;

//
// Workload setup
//

// This is the transaction runner class. It must implement run_txn(), which is
// called for each loop-iteration. In run_txn(), the workload must be generated,
// executed, and either aborted/retried if necessary. The return value is the
// number of successful operations (this is typically 1 for retry-loop workloads
// and successful no-retry workloads, and 0 for failed no-retry workloads).
template <typename DBParams>
class Adapting_runner : public db_common::common_runner<db_type<DBParams>> {
public:
    using base_runner = db_common::common_runner<db_type<DBParams>>;

    using base_runner::db;
    using typename base_runner::Access;

    template <typename T>
    using Record = typename base_runner::template Record<T>;

    template <typename... Args>
    Adapting_runner(Args&&... args)
        : base_runner(std::forward<Args>(args)...),
          key_gen(db.rng[base_runner::id], 0, db.params.table_size - 1) {
        ops.resize(db.params.ops_per_txn);
        auto& stats = get_stats(base_runner::id);
        memset(&stats, 0, sizeof stats);
    }

    // For convenience
    enum txn_type {
        read_only = 0, write_less, write_some, write_heavy
    };

    struct txn_params {
        txn_params() : key(0), txn() {}

        adapting_key key;
        txn_type txn;
    };

    // Needs to be aligned to avoid shared cache lines; we use this instead of
    // __thread storage to allow for accumulation on one thread at the end.
    struct __attribute__((aligned(128))) stats_type {
        int64_t txns_started;
        int64_t txns_committed;

        stats_type& operator +=(const stats_type& other) {
            txns_started += other.txns_started;
            txns_committed += other.txns_committed;
            return *this;
        }

        void report() {
            double abort_rate = (1.0 * txns_started - txns_committed) / txns_started * 100.0;
            char abort_rate_str[8];
            snprintf(abort_rate_str, 7, "%.2f%%", abort_rate);
            std::cout << "Transactions committed : " << txns_committed << std::endl
                      << "Total transactions     : " << txns_started << std::endl
                      << "Abort rate             : " << abort_rate_str << std::endl
                      ;
        }
    };

    // Generate a single transaction to run
    inline void generate_transaction() {
        // Generate keys
        for (auto itr = ops.begin(); itr != ops.end(); ++itr) {
            auto key = key_gen.sample_idx();
            itr->key = adapting_key(key);
            itr->txn = txn_type(key % db.params.variants);
        }
    }

    // Implement this!
    size_t run_txn() override {
        return txn_adapt();
    }

    // Transaction implementations
    size_t txn_adapt() {
        using nc = adapting_value::NamedColumn;

        bool success, result;
        uintptr_t row;

        generate_transaction();

        TXN {
            ++get_stats(base_runner::id).txns_started;
            Record<adapting_value> value;
            for (auto itr = ops.begin(); itr != ops.end(); ++itr) {
                const auto key = itr->key;
                const auto txn = itr->txn;

                switch (txn) {
                case write_heavy:
                {
                    std::tie(success, result, row, value) = base_runner::select(
                        db.tbl_adapting, key,
                        {{nc::read_only, Access::read},
                         {nc::write_some, DBParams::Commute ? Access::write : Access::update},
                         {nc::write_much, DBParams::Commute ? Access::write : Access::update},
                         {nc::write_most, DBParams::Commute ? Access::write : Access::update}},
                        3);
                    CHK(success);
                    assert(result);

                    if constexpr (DBParams::Commute) {
                        commutators::Commutator<adapting_value> comm(txn);
                        db.tbl_adapting.update_row(row, comm);
                    } else {
                        auto* new_value = Sto::tx_alloc<adapting_value>();
                        value.copy_into(new_value);
                        ++new_value->write_some;
                        ++new_value->write_much;
                        ++new_value->write_most;

                        db.tbl_adapting.update_row(row, new_value);
                    }
                    break;
                }
                case write_some:
                {
                    std::tie(success, result, row, value) = base_runner::select(
                        db.tbl_adapting, key,
                        {{nc::read_only, Access::read},
                         {nc::write_some, Access::read},
                         {nc::write_much, DBParams::Commute ? Access::write : Access::update},
                         {nc::write_most, DBParams::Commute ? Access::write : Access::update}},
                        2);
                    CHK(success);
                    assert(result);

                    if constexpr (DBParams::Commute) {
                        commutators::Commutator<adapting_value> comm(txn);
                        db.tbl_adapting.update_row(row, comm);
                    } else {
                        auto* new_value = Sto::tx_alloc<adapting_value>();
                        value.copy_into(new_value);
                        ++new_value->write_much;
                        ++new_value->write_most;

                        db.tbl_adapting.update_row(row, new_value);
                    }
                    break;
                }
                case write_less:
                {
                    std::tie(success, result, row, value) = base_runner::select(
                        db.tbl_adapting, key,
                        {{nc::read_only, Access::read},
                         {nc::write_some, Access::read},
                         {nc::write_much, Access::read},
                         {nc::write_most, DBParams::Commute ? Access::write : Access::update}},
                        1);
                    CHK(success);
                    assert(result);

                    if constexpr (DBParams::Commute) {
                        commutators::Commutator<adapting_value> comm(txn);
                        db.tbl_adapting.update_row(row, comm);
                    } else {
                        auto* new_value = Sto::tx_alloc<adapting_value>();
                        value.copy_into(new_value);
                        ++new_value->write_most;

                        db.tbl_adapting.update_row(row, new_value);
                    }
                    break;
                }
                case read_only:
                {
                    std::tie(success, result, row, value) = base_runner::select(
                        db.tbl_adapting, key,
                            {{nc::read_only, Access::read},
                             {nc::write_some, Access::read},
                             {nc::write_much, Access::read},
                             {nc::write_most, Access::read}},
                            0);
                    CHK(success);
                    assert(result);
                    break;
                }
                }
            }
            CHK(true);
        } TEND(true);  // Retry on abort

        ++get_stats(base_runner::id).txns_committed;
        return 1;
    }

    // Get the stats object for a given thread id; creates the object if it
    // doesn't already exist.
    static stats_type& get_stats(size_t thread_id) {
        static std::vector<stats_type> stats;
        if (thread_id >= stats.size()) {
            stats.resize(thread_id + 1);
        }
        return stats[thread_id];
    }

    // Runs after the experiment's default reporting has completed, but before
    // global garbage collection.
    void epilogue(bool is_first_thread, bool is_last_thread) override {
        static stats_type accumulator;
        if (is_first_thread) {
            memset(&accumulator, 0, sizeof accumulator);
        }
        auto& stats = get_stats(base_runner::id);
        accumulator += stats;
        if (is_last_thread) {
            accumulator.report();
        }
    }

    sampling::StoUniformDistribution<> key_gen;
    std::vector<txn_params> ops;
};
// This is an alias for the runner type. Or we could've just called it
// runner_type to begin with.
template <typename DBParams> using runner_type = Adapting_runner<DBParams>;

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

    parser.AddOption(
            "ops", 'o', Clp_ValLong, Clp_Optional,
            [] (CommandParams& params, std::any value) -> bool {
                // Cast the value back from std::any
                params.ops_per_txn = std::any_cast<long>(value);
                // Return true if the value is acceptable; false otherwise.
                return params.ops_per_txn > 0;
            },
            "Number of operations per transaction (default 1000).");

    parser.AddOption(
            "table-size", 'z', Clp_ValLong, Clp_Optional,
            [] (CommandParams& params, std::any value) -> bool {
                // Cast the value back from std::any
                params.table_size = std::any_cast<long>(value);
                // Return true if the value is acceptable; false otherwise.
                return params.table_size > 0;
            },
            "Table size (default 1000000).");

    parser.AddOption(
            "op-variants", 'v', Clp_ValInt, Clp_Optional,
            [] (CommandParams& params, std::any value) -> bool {
                // Cast the value back from std::any
                auto variants = std::any_cast<int>(value);
                params.variants = static_cast<int8_t>(variants);
                // Return true if the value is acceptable; false otherwise.
                return variants > 0 && variants <= 4;
            },
            "Operation variants to include (default 4). Variants are:",
            "  read-only, write-heavy, write-some, write-less",
            "Including 2 variants means only using {read-only, write-heavy} operations,"
            "while including 3 variants means only using {read-only, write-heavy, write-some} operations.");

    // Go!
    return db_common::template run<bench_type>(parser);
}

};  // namespace adapting_bench

int main(int argc, const char * const *argv) {
    return adapting_bench::run(argc, argv);
}
