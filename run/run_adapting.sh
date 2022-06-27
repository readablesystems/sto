#!/bin/bash

METARUN="yes" run/run_adapting_100opt.sh
METARUN="yes" run/run_adapting_1000opt.sh
METARUN="yes" run/run_adapting_100opt_4sp.sh
METARUN="yes" run/run_adapting_1000opt_4sp.sh
cd results
join -t, --header --nocheck-order adapting_100opt_results.txt adapting_1000opt_results.txt > adapting_results.txt
python3 /home/ubuntu/send_email.py --exp="Adapting" adapting_results.txt
sudo shutdown -h +1
