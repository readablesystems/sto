#!/bin/bash

METARUN="yes" run/run_wiki_occ.sh
METARUN="yes" run/run_wiki_tictoc.sh
METARUN="yes" run/run_wiki_mvcc.sh
cd results
join -t, --header --nocheck-order wiki_occ_results.txt wiki_tictoc_results.txt | join -t, --header --nocheck-order - wiki_mvcc_results.txt > wiki_results.txt
python3 /home/ubuntu/send_email.py --exp="Wikipedia" wiki_results.txt
sudo shutdown -h +1
