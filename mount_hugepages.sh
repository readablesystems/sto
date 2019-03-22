#/bin/bash

# Huge pages are 2MiB by default on our system (and this script can
# only allocate huge pages in default sizes). Caculate the amount
# of memory roughly needed, divide it by 2MiB, and that's the
# number of pages to pass in to this script.

# Common examples:

# 32Gib = 16384 pages
# 64GiB = 32768 pages
# 96GiB = 49152 pages <- use this one for TPC-C
# 128GiB = 65536 pages

if [ $# -ge 1 ]; then
  echo $1 | sudo tee /proc/sys/vm/nr_hugepages
else
  echo "Too few parameters. Exiting not doing anything."
fi
