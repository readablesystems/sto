#!/bin/bash

METAVAR="yes" run/run_ycsb_a.sh
METAVAR="yes" run/run_ycsb_b.sh
METAVAR="yes" run/run_ycsb_c.sh
sudo shutdown -h +1
