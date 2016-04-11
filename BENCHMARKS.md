To run the benchmarks from the paper/reproduce the graphs:

Silo
----
    $ cd silo
    $ cd benchmarks/sto; ./bootstrap.sh; ./configure; cd -
    $ python datacollect.py OUTPUT_FILENAME
This will take a while (45 minutes or so). Once it's done
`python plot.py benchmarks/data/OUTPUT_FILENAME tpcc.pdf` will generate
a graph like in the paper in the file `tpcc.pdf`.

If you don't want to wait 45 minutes, you can run e.g.
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
Test results will be in results.txt

Microbenchmarks
---------------
    $ make concurrent-50 && make concurrent-1M
Now make sure the microbenchmark you wish to run is not commented out in
`microbenchmarks/run_benchmarks.py` and `microbenchmarks/get_data.py`, then run 

    $ python microbenchmarks/run_benchmarks.py 5
And either use the data in `experiment_data.json` directly or run:

    $ python microbenchmarks/get_data.py 5
to output a csv.
