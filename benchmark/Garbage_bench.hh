#include <thread>

#include "DB_index.hh"
#include "TBox.hh"
#include "TCounter.hh"
#include "DB_params.hh"

namespace garbage_bench {

struct garbage_row {
    enum class NamedColumn : int { value = 0 };
    int value;

    garbage_row() {}
    explicit garbage_row(int v) : value(v) {}
};

struct garbage_key {
    uint64_t k;

    explicit garbage_key(uint64_t p_k) : k(p_k) {}
    explicit garbage_key(lcdf::Str& mt_key) { 
        assert(mt_key.length() == sizeof(*this));
        memcpy(this, mt_key.data(), mt_key.length());
    }
    operator lcdf::Str() const {
        return lcdf::Str((char *)this, sizeof(*this));
    }
};

template <typename DBParams>
class garbage_db {
public:
    template <typename K, typename V>
    using OIndex = bench::ordered_index<K, V, DBParams>;
    typedef OIndex<garbage_key, garbage_row> table_type;

    table_type& table() {
        return tbl_;
    }
    size_t size() const {
        return size_;
    }
    size_t& size() {
        return size_;
    }
private:
    size_t size_;
    table_type tbl_;
};

template <typename DBParams>
void initialize_db(garbage_db<DBParams>& db, size_t db_size) {
    db.table().thread_init();
    for (size_t i = 0; i < db_size; i += 2)
        db.table().nontrans_put(garbage_key(i), garbage_row(0));
    db.size() = db_size;
}

template <typename DBParams>
class garbage_runner {
public:
    using RowAccess = bench::RowAccess;
    void run_txn(size_t key) {
        TRANSACTION_E {
            bool success, found;
            const void *value;
            std::tie(success, found, std::ignore, value) = db.table().select_row(garbage_key(key), RowAccess::ObserveValue);
            if (success) {
                if (found) {
                  std::tie(success, found) = db.table().delete_row(garbage_key(key));
                } else {
                  std::tie(success, found) = db.table().insert_row(garbage_key(key), Sto::tx_alloc<garbage_row>());
                }
            }
        } RETRY_E(true);
    }

    // returns the total number of transactions committed
    size_t run() {
        std::vector<std::thread> thrs;
        std::vector<size_t> txn_cnts((size_t)num_runners, 0);
        for (int i = 0; i < num_runners; ++i)
            thrs.emplace_back(
                &garbage_runner::runner_thread, this, i, std::ref(txn_cnts[i]));
        for (auto& t : thrs)
            t.join();
        size_t combined_txn_count = 0;
        for (auto c : txn_cnts)
            combined_txn_count += c;
        return combined_txn_count;
    }

    garbage_runner(int nthreads, double time_limit, garbage_db<DBParams>& database)
        : num_runners(nthreads), tsc_elapse_limit(), db(database) {
        using db_params::constants;
        tsc_elapse_limit = (uint64_t)(time_limit * constants::processor_tsc_frequency * constants::billion);
    }

private:
    void runner_thread(int runner_id, size_t& committed_txns) {
        ::TThread::set_id(runner_id);
        db.table().thread_init();
        std::mt19937 gen(runner_id);
        std::uniform_int_distribution<uint64_t> dist(0, db.size() - 1);

        size_t thread_txn_count = 0;
        auto tsc_begin = read_tsc();
        size_t key = dist(gen) % db.size();
        while (true) {
            run_txn(key);
            key = (key + 1) % db.size();
            ++thread_txn_count;
            if ((thread_txn_count & 0xfful) == 0) {
                if (read_tsc() - tsc_begin >= tsc_elapse_limit)
                    break;
            }
        }

        committed_txns = thread_txn_count;
    }

    int num_runners;
    uint64_t tsc_elapse_limit;
    garbage_db<DBParams> db;
};

};
