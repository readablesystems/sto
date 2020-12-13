#!/bin/bash

METARUN="yes" run/run_rubis_occ.sh
METARUN="yes" run/run_rubis_tictoc.sh
METARUN="yes" run/run_rubis_mvcc.sh
cd results
join -t, --header --nocheck-order rubis_occ_results.txt rubis_tictoc_results.txt | join -t, --header --nocheck-order - rubis_mvcc_results.txt > rubis_results.txt
python3 /home/ubuntu/send_email.py --exp="Rubis" rubis_results.txt
sudo shutdown -h +1
