#!/bin/bash

run/run_tpcc_occ.sh
run/run_tpcc_tictoc.sh
run/run_tpcc_mvcc.sh
#run/run_tpcc_mvcc_cu_past.sh
run/run_tpcc_factors.sh
run/run_tpcc_index_contention.sh
sudo shutdown -h +1
