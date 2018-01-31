#pragma once

#include "YCSB_structs.hh"
#include "TPCC_index.hh"


namespace ycsb {

template <typename DBParams>
class ycsb_db {
public:
    template <typename K, typename V>
	using OIndex = tpcc::ordered_index<K, V, DBParams>;

	typedef OIndex<ycsb_key, ycsb_value> ycsb_table_type;

	ycsb_table_type& ycsb_table() {
		return ycsb_table;
	}

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
			} else (mode_id == MediumContention) {
				dd = new StoSampling::StoZipfDistribution(thread_id, 0, TABLE_SIZE - 1, 0.8);
			} else (mode_id == HighContention) {
				dd = new StoSampling::StoZipfDistribution(thread_id, 0, TABLE_SIZE - 1, );
			}
	}


    inline txn_type next_transaction() {
        uint64_t x = ig.random(1, 100);
        if (x <= 49)
            return txn_type::new_order;
        else if (x <= 96)
            return txn_type::payment;
        else
            return txn_type::order_status;
    }

    inline void run_txn_read_only();
    inline void run_txn_medium_contention();
    inline void run_txn_high_contention();

private:
	ycsb_db<DBParams>& db;
	int runner_id;

    StoSampling::StoUniformDistribution *ud = (thread_id, 0, std::numeric_limits<uint32_t>::max());
	StoSampling::StoRandomDistribution *dd;
};


};