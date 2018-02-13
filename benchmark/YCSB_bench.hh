#pragma once

#include <iostream>
#include <string>

#include "compiler.hh"
#include "clp.h"
#include "sampling.hh"
#include "SystemProfiler.hh"
#include "YCSB_structs.hh"
#include "DB_index.hh"
#include "DB_params.hh"

namespace ycsb {

using bench::ordered_index;
using bench::unordered_index;

static constexpr uint64_t ycsb_table_size = 10000000;

template <typename DBParams>
class ycsb_db {
public:
    template <typename K, typename V>
    using OIndex = ordered_index<K, V, DBParams>;
    template <typename K, typename V>
    using UIndex = unordered_index<K, V, DBParams>;

    typedef UIndex<ycsb_key, ycsb_value<DBParams>> ycsb_table_type;

    explicit ycsb_db() : ycsb_table_(ycsb_table_size) {}

    ycsb_table_type& ycsb_table() {
        return ycsb_table_;
    }

    void table_thread_init() {
        //ycsb_table_.thread_init();
    }

    void prepopulate();

private:
    ycsb_table_type ycsb_table_;
};

struct ycsb_op_t {
    ycsb_op_t() : is_write(), key(), col_n() {}
    ycsb_op_t(bool w, uint32_t k, int32_t c)
            : is_write(w), key(k), col_n(c) {}
    bool is_write;
    uint32_t key;
    int32_t col_n;
};

template <typename DBParams>
class ycsb_runner {
public:
    typedef std::vector<ycsb_op_t> ycsb_txn_t;

    ycsb_runner(int tid, ycsb_db<DBParams>& database, mode_id mid)
        : db(database), ig(tid), runner_id(tid), mode(mid),
          ud(), dd(), write_threshold() {}

    inline void dist_init() {
        ud = new sampling::StoUniformDistribution(runner_id, 0, std::numeric_limits<uint32_t>::max());
        switch(mode) {
            case mode_id::ReadOnly:
                dd = new sampling::StoUniformDistribution(runner_id, 0, ycsb_table_size - 1);
                write_threshold = 0;
                break;
            case mode_id::MediumContention:
                dd = new sampling::StoZipfDistribution(runner_id, 0, ycsb_table_size - 1, 0.8);
                write_threshold = (uint32_t) (std::numeric_limits<uint32_t>::max()/10);
                break;
            case mode_id::HighContention:
                dd = new sampling::StoZipfDistribution(runner_id, 0, ycsb_table_size - 1, 0.9);
                write_threshold = (uint32_t) (std::numeric_limits<uint32_t>::max()/2);
                break;
            default:
                break;
        }
    }

    inline void gen_workload(int txn_size);

    int id() const {
        return runner_id;
    }

    inline void run_txn(const ycsb_txn_t& txn);

    std::vector<ycsb_txn_t> workload;

private:
    ycsb_db<DBParams>& db;
    ycsb_input_generator<DBParams> ig;
    int runner_id;
    mode_id mode;

    sampling::StoUniformDistribution *ud;
    sampling::StoRandomDistribution *dd;
    uint32_t write_threshold;
};

}; // namespace ycsb
