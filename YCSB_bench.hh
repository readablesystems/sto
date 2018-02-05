#pragma once

#include <iostream>
#include <string>

#include "compiler.hh"
#include "clp.h"
#include "sampling.hh"
#include "SystemProfiler.hh"
#include "YCSB_structs.hh"
#include "TPCC_index.hh"


namespace ycsb { 
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

    static constexpr uint64_t ycsb_table_size = 10000000;

    template <typename DBParams>
    class ycsb_db {
    public:
        template <typename K, typename V>
        using OIndex = tpcc::ordered_index<K, V, DBParams>;

        typedef OIndex<ycsb_key, ycsb_value> ycsb_table_type;

        ycsb_table_type& ycsb_table() {
            return ycsb_table_;
        }

        void prepopulate();

    private:
        ycsb_table_type ycsb_table_;
    };

    template <typename DBParams>
    class ycsb_runner {
    public:
        typedef std::vector<std::pair<bool, uint32_t>> ycsb_txn_t;

        ycsb_runner(int tid, ycsb_db<DBParams>& database, mode_id mid)
            : db(database), ig(tid), runner_id(tid) {
            ud = new StoSampling::StoUniformDistribution(tid, 0, std::numeric_limits<uint32_t>::max());
            switch(mid) {
                case mode_id::ReadOnly:
                    dd = new StoSampling::StoUniformDistribution(tid, 0, ycsb_table_size - 1);
                    write_threshold = 0;
                    break;
                case mode_id::MediumContention:
                    dd = new StoSampling::StoZipfDistribution(tid, 0, ycsb_table_size - 1, 0.8);
                    write_threshold = (uint32_t) (std::numeric_limits<uint32_t>::max()/10);
                    break;
                case mode_id::HighContention:
                    dd = new StoSampling::StoZipfDistribution(tid, 0, ycsb_table_size - 1, 0.9);
                    write_threshold = (uint32_t) (std::numeric_limits<uint32_t>::max()/2);
                    break;
                default:
                    break;
            }
        }

        inline void gen_workload(int txn_size);

        inline void run_txn(const ycsb_txn_t& txn);

        std::vector<ycsb_txn_t> workload;

    private:
        ycsb_db<DBParams>& db;
        ycsb_input_generator ig;
        int runner_id;

        StoSampling::StoUniformDistribution *ud;
        StoSampling::StoRandomDistribution *dd;
        uint32_t write_threshold;
    };

};
