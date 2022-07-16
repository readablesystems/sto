#!/bin/bash

METARUN="yes" run/run_adapting_2sp.sh
METARUN="yes" run/run_adapting_3sp.sh
METARUN="yes" run/run_adapting_4sp.sh
cd results
join -t, --header --nocheck-order adapting_2sp_results.txt adapting_3sp_results.txt | join -t, --header --nocheck-order - adapting_4sp_results.txt > adapting_results.txt
python3 /home/ubuntu/send_email.py --exp="Adapting" adapting_results.txt
sudo shutdown -h +1
