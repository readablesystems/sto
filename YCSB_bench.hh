#pragma once

#include <iostream>
#include <string>

#include "compiler.hh"
#include "clp.h"
#include "SystemProfiler.hh"
#include "YCSB_structs.hh"
#include "TPCC_index.hh"


namespace ycsb {

    class constants {
    public:
        static constexpr double million = 1000000.0;
        static constexpr double billion = 1000.0 * million;
        static double processor_tsc_frequency; // in GHz
    };

    #define TABLE_SIZE 10000000;

    template <typename DBParams>
    class ycsb_db {
    public:
        template <typename K, typename V>
        using OIndex = tpcc::ordered_index<K, V, DBParams>;

        typedef OIndex<ycsb_key, ycsb_value> ycsb_table_type;

        ycsb_table_type& ycsb_table() {
            return ycsb_table;
        }

        void prepopulate(ycsb_db<DBParams> db);

    private:
        ycsb_table_type ycsb_table;
    };


    template <typename DBParams>
    class ycsb_runner {

        ycsb_runner(int id, ycsb_db<DBParams>& database, int mode_id)
            : db(database), runner_id(id) {
            ud = new StoSampling::StoUniformDistribution(thread_id, 0, std::numeric_limits<uint32_t>::max());
            if (mode_id == ReadOnly) {
                dd = new StoSampling::StoUniformDistribution(thread_id, 0, TABLE_SIZE - 1);
                write_threshold = 0;
            } else (mode_id == MediumContention) {
                dd = new StoSampling::StoZipfDistribution(thread_id, 0, TABLE_SIZE - 1, 0.8);
                write_threshold = (uint32_t)(std::numeric_limits<uint32_t>::max() * 0.1);
            } else (mode_id == HighContention) {
                dd = new StoSampling::StoZipfDistribution(thread_id, 0, TABLE_SIZE - 1, 0.9);
                write_threshold = (uint32_t)(std::numeric_limits<uint32_t>::max() * 0.5);
            }
        }

        inline void run_txn_read_only();
        inline void run_txn_medium_contention();
        inline void run_txn_high_contention();

    private:
        ycsb_db<DBParams>& db;
        int runner_id;

        StoSampling::StoUniformDistribution *ud = (thread_id, 0, std::numeric_limits<uint32_t>::max());
        StoSampling::StoRandomDistribution *dd;
        uint32_t write_threshold;
    };
};