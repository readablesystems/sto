#! /bin/sh

make cds_benchmarks
for i in `seq 0 11`; do
    perf record -e cache-misses -c 1000 ./cds_benchmarks $i
    perf report | head -19 | tail -3 > $i.cachemisses.out
done
for i in `seq 110 121`; do
    perf record -e cache-misses -c 1000 ./cds_benchmarks $i
    perf report | head -19 | tail -3 > $i.cachemisses.out
done
for i in `seq 210 221`; do
    perf record -e cache-misses -c 1000 ./cds_benchmarks $i
    perf report | head -19 | tail -3 > $i.cachemisses.out
done
