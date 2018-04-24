#include <thread>

#include "DB_index.hh"
#include "TBox.hh"
#include "TCounter.hh"
#include "DB_params.hh"

namespace predicate_bench {

template <typename DBRow> struct predicate_row {
    enum class NamedColumn : int { balance = 0 };
    DBRow balance;

    predicate_row() {}
    explicit predicate_row(int v) : balance(v) {}
};

struct predicate_key {
    uint64_t k;

    explicit predicate_key(uint64_t p_k) : k(p_k) {}
    explicit predicate_key(lcdf::Str& mt_key) { 
        assert(mt_key.length() == sizeof(*this));
        memcpy(this, mt_key.data(), mt_key.length());
    }
    operator lcdf::Str() const {
        return lcdf::Str((char *)this, sizeof(*this));
    }
};

template <typename DBParams, typename DBRow>
class predicate_db {
public:
    template <typename K, typename V>
    using OIndex = bench::ordered_index<K, V, DBParams>;
    typedef OIndex<predicate_key, predicate_row<DBRow>> table_type;

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

template <typename DBParams, typename DBRow>
void initialize_db(predicate_db<DBParams, DBRow>& db, size_t db_size) {
    db.table().thread_init();
    for (size_t i = 0; i < db_size; ++i)
        db.table().nontrans_put(predicate_key(i), predicate_row<DBRow>(1000));
    db.size() = db_size;
}

template <typename DBParams, typename DBRow>
class predicate_runner {
public:
    using RowAccess = bench::RowAccess;
    void run_txn(size_t key) {
        TRANSACTION_E {
            bool abort, result;
            const void *value;
            std::tie(abort, result, std::ignore, value) = db.table().select_row(predicate_key(key), RowAccess::ObserveValue);
            auto r = reinterpret_cast<predicate_row<DBRow> *>(const_cast<void *>(value));
            if (r->balance > 10) {
                r->balance = r->balance - 1;
            } else {
                r->balance = r->balance + 1000;
            }
        } RETRY_E(true);
    }

    void run_ro_txn(size_t key) {
        TRANSACTION_E {
            bool abort, result;
            const void *value;
            std::tie(abort, result, std::ignore, value) = db.table().select_row(predicate_key(key), RowAccess::ObserveValue);
            auto r = reinterpret_cast<predicate_row<DBRow> *>(const_cast<void *>(value));
            volatile auto s = r->balance + 1000;
            (void)s;  // Prevent s from being optimized away
        } RETRY_E(true);
    }

    // returns the total number of transactions committed
    size_t run(const bool readonly) {
        std::vector<std::thread> thrs;
        std::vector<size_t> txn_cnts((size_t)num_runners, 0);
        for (int i = 0; i < num_runners; ++i)
            thrs.emplace_back(
                &predicate_runner::runner_thread, this, i, std::ref(txn_cnts[i]),
                readonly);
        for (auto& t : thrs)
            t.join();
        size_t combined_txn_count = 0;
        for (auto c : txn_cnts)
            combined_txn_count += c;
        return combined_txn_count;
    }

    predicate_runner(int nthreads, double time_limit, predicate_db<DBParams, DBRow>& database)
        : num_runners(nthreads), tsc_elapse_limit(), db(database) {
        using db_params::constants;
        tsc_elapse_limit = (uint64_t)(time_limit * constants::processor_tsc_frequency * constants::billion);
    }

private:
    void runner_thread(int runner_id, size_t& committed_txns, bool readonly) {
        ::TThread::set_id(runner_id);
        db.table().thread_init();
        std::mt19937 gen(runner_id);
        std::uniform_int_distribution<uint64_t> dist(0, db.size() - 1);

        size_t thread_txn_count = 0;
        auto tsc_begin = read_tsc();
        while (true) {
            if (readonly) {
              run_ro_txn(dist(gen));
            } else {
              run_txn(dist(gen));
            }
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
    predicate_db<DBParams, DBRow> db;
};

};
