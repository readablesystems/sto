#!/bin/bash

METAVAR="yes" run/run_tpcc_occ.sh
METAVAR="yes" run/run_tpcc_tictoc.sh
METAVAR="yes" run/run_tpcc_mvcc.sh
#METAVAR="yes" run/run_tpcc_mvcc_cu_past.sh
METAVAR="yes" run/run_tpcc_factors.sh
METAVAR="yes" run/run_tpcc_index_contention.sh
sudo shutdown -h +1
