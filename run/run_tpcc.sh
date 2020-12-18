#!/bin/bash

METARUN="yes" run/run_tpcc_occ.sh
METARUN="yes" run/run_tpcc_tictoc.sh
METARUN="yes" run/run_tpcc_mvcc.sh
#METARUN="yes" run/run_tpcc_mvcc_cu_past.sh
METARUN="yes" run/run_tpcc_factors.sh
METARUN="yes" run/run_tpcc_index_contention.sh
sudo shutdown -h +1
