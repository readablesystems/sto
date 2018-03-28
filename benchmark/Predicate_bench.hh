#include "DB_index.hh"
#include "TCounter.hh"

namespace predicate_bench {

struct predicate_row {
    TCounter<int, TNonopaqueWrapped<int>> balance;

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
    operator lcdf::Str() {
        return lcdf::Str((char *)this, sizeof(*this));
    }
};

template <typename DBParams>
class predicate_db {
public:
    template <typename K, typename V>
    using OIndex = bench::ordered_index<K, V, DBParams>;
    typedef OIndex<predicate_key, predicate_row> table_type;

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
void initialize_db(predicate_db& db, size_t db_size) {
    db.table().thread_init();
    for (size_t i = 0; i < db_size; ++i)
        db.table().nontrans_insert(predicate_key(i), predicate_row(1000));
    db.size() = db_size;
}

template <typename DBParams>
class predicate_runner {
public:
    void run_txn(size_t key) {
        TRANSACTION {
            bool abort, result;
            const void *value;
            std::tie(abort, result, std::ignore, value) = db.table().select_row(predicate_key(key), false);
            auto r = const_cast<predicate_row *>(value);
            if (r->balance > 10) {
                r->balance -= 1;
            } else {
                r->balance += 1000;
            }
        } RETRY(true);
    }

    void run() {
        std::vector<std::thread> thrs;
        for (int i = 0; i < num_runners; ++i) {
            thrs.emplace_back(&predicate_runner::runner_thread, this, i);
        }
        for (auto& t : thrs)
            thrs.join();
    }

    predicate_runner(int nthreads, size_t ntrans_thread, predicate_db<DBParams>& d)
        : num_runners(nthreads), db(d) {}

private:
    void runner_thread(int runner_id) {
        db.table().thread_init();
        std::mt19937 gen(runner_id);
        std::uniform_int_distribution<uint64_t> dist(0, db.size() - 1);
        for (size_t i = 0; i < ntrans_thread; ++i) {
            run_txn(dist(gen));
        }
    }

    int num_runners;
    size_t ntrans_thread;
    predicate_db<DBParams> db;
};

};
