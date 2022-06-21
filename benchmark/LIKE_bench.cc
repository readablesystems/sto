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
#include "LIKE_structs_generated.hh"

// Declare a namespace -- it's good practice for code isolation!
namespace like_bench {

//
// Data structures
//

using page_key = db_common::keys::single_key<int64_t>;
using like_key = db_common::keys::single_key<int64_t>;

//
// Command line parameters
//

// This is the structure for holding additional command line parameters. Make
// sure to pass it in as the template argument to the parser class!
struct CommandParams : db_common::ClpCommandParams {
    enum ContentionAlgorithm {
        DEFAULT = 0, ZIPF
    };

    static constexpr const char* stringof(ContentionAlgorithm alg) {
        switch (alg) {
        case DEFAULT:
            return "default";
        case ZIPF:
            return "zipf";
        }
        return "unknown";
    }

    explicit CommandParams()
        : db_common::ClpCommandParams(),
          contention(DEFAULT), skew(1), read_rate(0), noncontended_read_rate(80),
          user_count(1000000)
          {}

    ssize_t page_count() const {
        switch (contention) {
        case DEFAULT:
            return user_count / skew;
        case ZIPF:
            return user_count;
        }
        return 0;
    }

    void print() override {
        db_common::ClpCommandParams::print();
        std::cout << "Hot key algorithm      : " << stringof(contention) << std::endl
                  << "User count             : " << user_count << std::endl
                  << "Hot page count         : " << page_count() << std::endl
                  << "Read rate              : " << read_rate << std::endl
                  << "Noncontended read rate : " << noncontended_read_rate << std::endl
                  << "Skew parameter         : " << skew << std::endl
                  ;
    }

    ContentionAlgorithm contention;
    double skew;
    double read_rate;
    double noncontended_read_rate;
    ssize_t user_count;
};

//
// Database Setup
//

// This is the database type. It defines the tables, too.
template <typename DBParams>
class LIKE_db : public db_common::common_db<DBParams> {
public:
    using base_db = db_common::common_db<DBParams>;
    template <typename K, typename V>
    using OIndex = typename base_db::template OIndex<K, V>;
    template <typename K, typename V>
    using UIndex = typename base_db::template UIndex<K, V>;

    using base_db::rng;

    template <typename CommandParams>
    LIKE_db(CommandParams& params)
        : base_db(params), params(params),
          tbl_page(params.user_count), tbl_like(params.user_count) {}

    template <typename CommandParams, typename RunParams>
    void setup(CommandParams&, RunParams&) {
        // Prepopulation goes in here
        static uint64_t nthreads = topo_info.num_cpus;
        static ssize_t user_block_size = params.user_count / nthreads;
        static ssize_t page_block_size = params.user_count / nthreads;

        std::vector<int64_t> prepop_count(nthreads, 0);

        base_db::prepopulate([&] (uint64_t thread_id) {
            // Choose a distribution
            // ex: Initially like a random page
            auto like_dist = sampling::StoUniformDistribution<>(rng[thread_id], 0, params.user_count - 1);
            // ex: Initial like counts range [1, 100]
            auto page_dist = sampling::StoUniformDistribution<>(rng[thread_id], 1, 100);

            // Set the bounds for each thread
            ssize_t start_id = thread_id * user_block_size;
            ssize_t end_id = (thread_id + 1 < nthreads ?
                (thread_id + 1) * user_block_size : params.user_count) - 1;

            // Prepopulate with data
            for (int32_t user_id = start_id; user_id <= end_id; ++user_id) {
                int32_t page_id = like_dist.sample_idx();
                like_value value = { .user_id = user_id, .page_id = page_id };
                tbl_like.nontrans_put(like_key(user_id), value);
            }

            // Set the bounds for each thread
            start_id = thread_id * page_block_size;
            end_id = (thread_id + 1 < nthreads ?
                (thread_id + 1) * page_block_size : params.user_count) - 1;

            // Prepopulate with data
            for (int32_t page_id = start_id; page_id <= end_id; ++page_id) {
                int32_t likes = page_dist.sample_idx();
                page_value value = { .page_id = page_id, .likes = likes };
                tbl_page.nontrans_put(page_key(page_id), value);
                prepop_count[thread_id] += likes;
            }
        });

        prepopulate_data.page_likes = 0;
        for (auto count : prepop_count) {
            prepopulate_data.page_likes += count;
        }

        std::cout << "Prepopulated pages with " << prepopulate_data.page_likes
                  << " initial likes." << std::endl;
    }

    void thread_init_all() override {
        base_db::thread_init(tbl_page);
        base_db::thread_init(tbl_like);
    }

    const CommandParams params;
    struct {
        int64_t page_likes;
    } prepopulate_data;

    OIndex<page_key, page_value> tbl_page;
    UIndex<like_key, like_value> tbl_like;
};
// This is an alias for the DB type. Or we could've just called it db_type to
// begin with.
template <typename DBParams> using db_type = LIKE_db<DBParams>;

//
// Workload setup
//

// This is the transaction runner class. It must implement run_txn(), which is
// called for each loop-iteration. In run_txn(), the workload must be generated,
// executed, and either aborted/retried if necessary. The return value is the
// number of successful operations (this is typically 1 for retry-loop workloads
// and successful no-retry workloads, and 0 for failed no-retry workloads).
template <typename DBParams>
class LIKE_runner : public db_common::common_runner<db_type<DBParams>> {
public:
    using base_runner = db_common::common_runner<db_type<DBParams>>;

    using base_runner::db;
    template <typename T>
    using Record = typename base_runner::template Record<T>;

    template <typename... Args>
    LIKE_runner(Args&&... args)
        : base_runner(std::forward<Args>(args)...),
          txn_gen(db.rng[0], 0, 99),
          user_gen(db.rng[0], 0, db.params.user_count - 1),
          default_page_gen(db.rng[0], 0, db.params.page_count() - 1),
          zipf_page_gen(db.rng[0], 0, db.params.page_count() - 1, db.params.skew) {}

    // For convenience
    enum txn_type {
        read_hot = 0, read_cold, like_page
    };

    inline txn_type generate_transaction() {
        // Generate a single transaction to run
        auto p_txn = txn_gen.sample_idx();
        if (p_txn < db.params.read_rate) {
            p_txn = txn_gen.sample_idx();
            if (p_txn < db.params.noncontended_read_rate) {
                return read_cold;
            }
            return read_hot;
        }
        return like_page;
    }

    inline std::tuple<uint64_t, uint64_t> generate_user_page(bool hotkey=true) {
        uint64_t user_id = user_gen.sample_idx();
        uint64_t page_id = 0;
        if (hotkey) {
            switch (db.params.contention) {
            case CommandParams::ContentionAlgorithm::DEFAULT:
                page_id = default_page_gen.sample_idx();
                break;
            case CommandParams::ContentionAlgorithm::ZIPF:
                page_id = zipf_page_gen.sample_idx();
                break;
            }
        } else {
            page_id = user_gen.sample_idx();
        }
        return {user_id, page_id};
    }

    // Implement this!
    size_t run_txn() override {
        switch (generate_transaction()) {
        case read_cold:
        {
            auto [user_id, page_id] = generate_user_page(false);
            return txn_read(user_id, page_id);
        }
        case read_hot:
        {
            auto [user_id, page_id] = generate_user_page(true);
            return txn_read(user_id, page_id);
        }
        case like_page:
        {
            auto [user_id, page_id] = generate_user_page(true);
            return txn_like_page(user_id, page_id);
        }
        }
        return 0;
    }

    // Transaction implementations
    size_t txn_read(uint64_t user_id, uint64_t page_id) {
        bool success, result;
        uintptr_t row;
        TXN {
            {
                Record<like_value> value;
                if constexpr (DBParams::Split == db_params::db_split_type::Adaptive) {
                    std::tie(success, result, row, value) = db.tbl_like.select_row(
                        like_key(user_id),
                        {{like_value::NamedColumn::user_id, AccessType::read},
                         {like_value::NamedColumn::page_id, AccessType::read}},
                        1);
                } else {
                    std::tie(success, result, row, value) = db.tbl_like.select_split_row(
                        like_key(user_id),
                        {{like_value::NamedColumn::user_id, bench::access_t::read},
                         {like_value::NamedColumn::page_id, bench::access_t::read}});
                }
                CHK(success);
                assert(result);
                (void)result;
                (void)value.user_id();
                (void)value.page_id();
            }

            {
                Record<page_value> value;
                if constexpr (DBParams::Split == db_params::db_split_type::Adaptive) {
                    std::tie(success, result, row, value) = db.tbl_page.select_row(
                        page_key(page_id),
                        {{page_value::NamedColumn::page_id, AccessType::read},
                         {page_value::NamedColumn::likes, AccessType::read}},
                        1);
                } else {
                    std::tie(success, result, row, value) = db.tbl_page.select_split_row(
                        page_key(page_id),
                        {{page_value::NamedColumn::page_id, bench::access_t::read},
                         {page_value::NamedColumn::likes, bench::access_t::read}});
                }
                CHK(success);
                assert(result);
                (void)result;
                (void)value.page_id();
                (void)value.likes();
            }
        } TEND(true);
        return 1;
    }
    size_t txn_like_page(uint64_t user_id, uint64_t page_id) {
        bool success, result;
        uintptr_t row;
        TXN {
            {
                Record<like_value> value;
                if constexpr (DBParams::Split == db_params::db_split_type::Adaptive) {
                    std::tie(success, result, row, value) = db.tbl_like.select_row(
                        like_key(user_id),
                        {{like_value::NamedColumn::user_id, AccessType::read},
                         {like_value::NamedColumn::page_id, AccessType::write}},
                        1);
                } else {
                    std::tie(success, result, row, value) = db.tbl_like.select_split_row(
                        like_key(user_id),
                        {{like_value::NamedColumn::user_id, bench::access_t::read},
                         {like_value::NamedColumn::page_id, bench::access_t::write}});
                }
                CHK(success);
                assert(result);
                (void)result;
                like_value* new_value = Sto::tx_alloc<like_value>();
                value.copy_into(new_value);
                new_value->page_id = page_id;
                db.tbl_like.update_row(row, new_value);
            }

            {
                Record<page_value> value;
                if constexpr (DBParams::Split == db_params::db_split_type::Adaptive) {
                    std::tie(success, result, row, value) = db.tbl_page.select_row(
                        page_key(page_id),
                        {{page_value::NamedColumn::page_id, AccessType::read},
                         {page_value::NamedColumn::likes, AccessType::update}},
                        1);
                } else {
                    std::tie(success, result, row, value) = db.tbl_page.select_split_row(
                        page_key(page_id),
                        {{page_value::NamedColumn::page_id, bench::access_t::read},
                         {page_value::NamedColumn::likes, bench::access_t::update}});
                }
                CHK(success);
                assert(result);
                (void)result;
                page_value* new_value = Sto::tx_alloc<page_value>();
                value.copy_into(new_value);
                ++new_value->likes;
                db.tbl_page.update_row(row, new_value);
            }
        } TEND(true);
        return 1;
        }

    sampling::StoUniformDistribution<> txn_gen;
    sampling::StoUniformDistribution<> user_gen;
    sampling::StoUniformDistribution<> default_page_gen;
    sampling::StoZipfDistribution<> zipf_page_gen;
};
// This is an alias for the runner type. Or we could've just called it
// runner_type to begin with.
template <typename DBParams> using runner_type = LIKE_runner<DBParams>;

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
            "contention-algorithm", 'a', Clp_ValString, Clp_Optional,
            [] (CommandParams& params, std::any value) -> bool {
                const char* str = std::any_cast<const char*>(value);
                if (!strncmp(str, "default", 8)) {
                    params.contention = CommandParams::ContentionAlgorithm::DEFAULT;
                } else if (!strncmp(str, "zipf", 5)) {
                    params.contention = CommandParams::ContentionAlgorithm::ZIPF;
                } else {
                    std::cerr << "Invalid contention algorithm: " << str << std::endl;
                    return false;
                }
                return true;
            },
            "Contention algorithm to use. Can be one of:",
            "  default, zipf");
    parser.AddOption(
            "skew", 'k', Clp_ValDouble, Clp_Optional,
            [] (CommandParams& params, std::any value) -> bool {
                params.skew = std::any_cast<double>(value);
                if (params.skew < 0) {
                    std::cerr << "Invalid skew parameter: " << params.skew << std::endl;
                    return false;
                }
                return true;
            },
            "Skew parameter (default 1). Meaning depends on contention algorithm:",
            "  [default]: skew parameter indicates the probability [0, 1] that",
            "             the hot key is in the transaction.",
            "  [zipf]:    skew parameter is the Zipfian distribution exponent",
            "             parameter. A value of 1 means only one hot key.");

    parser.AddOption(
            "read-rate", 'r', Clp_ValDouble, Clp_Optional,
            [] (CommandParams& params, std::any value) -> bool {
                params.read_rate = std::any_cast<double>(value);
                if (params.read_rate < 0 || params.read_rate > 100) {
                    std::cerr << "Invalid read rate: " << params.read_rate << std::endl;
                    return false;
                }
                return true;
            },
            "Read rate percent between 0 and 100 (default 0).");
    parser.AddOption(
            "noncontended-read-rate", 'n', Clp_ValDouble, Clp_Optional,
            [] (CommandParams& params, std::any value) -> bool {
                params.noncontended_read_rate = std::any_cast<double>(value);
                if (params.noncontended_read_rate < 0 || params.noncontended_read_rate > 100) {
                    std::cerr << "Invalid noncontended read rate: " << params.noncontended_read_rate << std::endl;
                    return false;
                }
                return true;
            },
            "Percentage [0, 100] of reads that are uncontended (default 80).");

    parser.AddOption(
            "user-count", 'u', Clp_ValInt, Clp_Optional,
            [] (CommandParams& params, std::any value) -> bool {
                params.user_count = std::any_cast<ssize_t>(value);
                if (params.user_count < 1) {
                    std::cerr << "Must have at least one user." << std::endl;
                    return false;
                }
                return true;
            },
            "Number of users and pages in the system (default 1,000,000).");

    // Go!
    return db_common::template run<bench_type>(parser);
}

};  // namespace like_bench

int main(int argc, const char * const *argv) {
    return like_bench::run(argc, argv);
}
