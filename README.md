Install
-------
```
    $ ./bootstrap.sh
    $ ./configure
    $ make
```
(NOTE: if you are using OS X you should probably run `./configure CXX='clang++ -stdlib=libc++ -std=c++11'`)

Tests
-----
Run single-threaded tests:

`./single`

Multi-threaded test: (`-c` performs a check of the multithreaded correctness)

`./concurrent randomrw DATASTRUCTURE -c`

^ runs with 4 threads, 1 million total transactions of size 10 each, half of 
which are read-writes

If this is too fast:

`./concurrent randomrw DATASTRUCTURE --ntrans=10000000 -c`

Check delete multithreaded correctness (not applicable for arrays):

`./concurrent xordelete DATASTRUCTURE -c`

You can get a list of both available tests and data structures by
running `./concurrent` without arguments.

Benchmark singlethreaded:

`./concurrent randomrw DATASTRUCTURE --nthreads=1`

or

`./concurrent randomrw DATASTRUCTURE --nthreads=1 --ntrans=10000000`

Or if you want to test transaction system overhead, read-my-writes overhead, 
etc. (larger transactions):

`./concurrent randomrw DATASTRUCTURE --nthreads=1 --opspertrans=100`

(and benchmarking multithreaded just involves changing the --nthreads argument)

Paper benchmarks
----------------
See `BENCHMARKS.md` for some help on reproducing data/graphs from the paper.
