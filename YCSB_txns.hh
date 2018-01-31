#pragma once

namespace ycsb {

#define TABLE_SIZE 10000000;


template <typename DBParams>
void ycsb_runner<DBParams>::run_txn_read_only() {

	dd = new StoSampling::StoUniformDistribution(thread_id, 0, TABLE_SIZE - 1);


	TRANSACTION {

		bool abort, result;
		uintptr_t row;
		const void *value;

		for (int i = 0; i < 2; i++) {
			std::tie(abort, result, row, value) = db.ycsb_table().select_row(ycsb_key(dd->sample()), false);
			TXN_DO(abort);
			assert(result);		
		}

    	(void)__txn_committed;

    // commit txn
	// retry until commits
	} RETRY(true);
}

template <typename DBParams>
void ycsb_runner<DBParams>::run_txn_medium_contention() {

	dd = new StoSampling::StoZipfDistribution(thread_id, 0, TABLE_SIZE - 1, 0.8);

	TRANSACTION {
		for (int i = 0; i < 16; i++) {
			if (ud.sample < )
		}




	} RETRY(true);
}

template <typename DBParams>
void ycsb_runner<DBParams>::run_txn_high_contention() {
	TRANSACTION {




	} RETRY(true);
}



};