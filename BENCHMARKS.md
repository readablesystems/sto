To run the benchmarks from the paper/reproduce the graphs:

Silo
----
    $ cd silo
    $ cd benchmarks/sto; ./bootstrap.sh; ./configure; cd -
    $ python datacollect.py OUTPUT_FILENAME
This will take a while (45 minutes or so). Once it's done
`python plot.py benchmarks/data/OUTPUT_FILENAME tpcc.pdf` will generate
a graph like in the paper in the file `tpcc.pdf`.

If you don't want to wait 45 minutes, you can run i.e.
    $ MODE=perf make -j dbtest
    $ out-perf.masstree/benchmarks/dbtest --runtime 30 --num-threads 16 --scale-factor 16 --bench tpcc -dmbta
which will run STO on the TPCC benchmark once with 16 threads and print the
resulting transactional throughput.

STAMP
-----
    $ cd stamp
    $ ./bootstrap.sh
    $ cd tl2
    $ make
    $ cd ../stamp-0.9.10
    $ cd BENCHMARK_OF_YOUR_CHOICE
    $ python script.py

Microbenchmarks
---------------
    $ git checkout ubenchmark
    $ make concurrent
    $ mv concurrent microbenchmarks/concurrent-1M
Now make sure the microbenchmark you wish to run is not commented out in
`microbenchmarks/run_benchmarks.py`, then run 
    $ python microbenchmarks/run_benchmarks.py 5
    $ python microbenchmarks/get_data.py 5
and the raw json data for the benchmark will be in 
`microbenchmarks/processed_MICROBENCHMARK_NAME.json`.