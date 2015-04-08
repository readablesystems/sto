Run single-threaded tests:
./single

Multi-threaded test:
Check multithreaded correctness:
./concurrent 3 DATASTRUCTURE -c
^ runs with 4 threads, 1 million total transactions of size 10 each, half of 
which are read-writes
If this is too fast:
./concurrent 3 DATASTRUCTURE --ntrans=10000000 -c
Check delete multithreaded correctness (not applicable for arrays):
./concurrent xordelete DATASTRUCTURE -c

You can get a list of both available tests and data structures by
running ./concurrent without arguments.

Note that the list tests are O(n^2) and generally not that great. Probably the
only one worth running is ./concurrent xordelete list -c --prepopulate=0

Benchmark singlethreaded:
./concurrent 3 DATASTRUCTURE --nthreads=1
or
./concurrent 3 DATASTRUCTURE --nthreads=1 --ntrans=10000000
Or if you want to test transaction system overhead, read-my-writes overhead, 
etc. (larger transactions):
./concurrent 3 DATASTRUCTURE --nthreads=1 --opspertrans=100

(and benchmarking multithreaded just involves changing the --nthreads argument)