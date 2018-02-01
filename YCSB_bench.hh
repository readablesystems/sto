#pragma once

#include <iostream>
#include <string>

#include "compiler.hh"
#include "clp.h"
#include "SystemProfiler.hh"
#include "YCSB_structs.hh"
#include "TPCC_index.hh"


namespace ycsb { 
    constexpr const char *db_params_id_names[] = {"none", "default", "opaque", "2pl", "adaptive", "swiss", "tictoc"};

    constexpr const char *db_params_id_names[] = {"none", "default", "opaque", "2pl", "adaptive", "swiss", "tictoc"};

    enum class db_params_id : int { None = 0, Default, Opaque, TwoPL, Adaptive, Swiss, TicToc };

    std::ostream& operator<<(std::ostream& os, const db_params_id& id) {
        os << db_params_id_names[static_cast<int>(id)];
        return os;
    }

    inline db_params_id parse_dbid(const char *id_string) {
        if (id_string == nullptr)
            return db_params_id::None;
        for (size_t i = 0; i < sizeof(db_params_id_names); ++i) {
            if (strcmp(id_string, db_params_id_names[i]) == 0) {
                auto selected = static_cast<db_params_id>(i);
                std::cout << "Selected \"" << selected << "\" as DB concurrency control." << std::endl;
                return selected;
            }
        }
        return db_params_id::None;
    }

    class db_default_params {
    public:
        static constexpr db_params_id Id = db_params_id::Default;
        static constexpr bool RdMyWr = false;
        static constexpr bool TwoPhaseLock = false;
        static constexpr bool Adaptive = false;
        static constexpr bool Opaque = false;
        static constexpr bool Swiss = false;
        static constexpr bool TicToc = false;
    };

    class db_opaque_params : public db_default_params {
    public:
        static constexpr db_params_id Id = db_params_id::Opaque;
        static constexpr bool Opaque = true;
    };

    class db_2pl_params : public db_default_params {
    public:
        static constexpr db_params_id Id = db_params_id::TwoPL;
        static constexpr bool TwoPhaseLock = true;
    };

    class db_adaptive_params : public db_default_params {
    public:
        static constexpr db_params_id Id = db_params_id::Adaptive;
        static constexpr bool Adaptive = true;
    };

    class db_swiss_params : public db_default_params {
    public:
        static constexpr db_params_id Id = db_params_id::Swiss;
        static constexpr bool Swiss = true;
    };

    class db_tictoc_params : public db_default_params {
    public:
        static constexpr db_params_id Id = db_params_id::TicToc;
        static constexpr bool TicToc = true;
    };

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
            } else if (mode_id == MediumContention) {
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
