#include <string>
#include <iostream>
#include <vector>
#include <random>
#include <map>
#include <iostream>
#include <cassert>

#include "Transaction.hh"
#include "RadixTree.hh"

#define GLOBAL_SEED 11
#define MAX_VALUE  100000
#define NTRANS 1000
#define N_THREADS 4

typedef std::map<uint64_t, uint64_t> reference_t;
typedef RadixTree<uint64_t, uint64_t> candidate_t;

bool check_tree(reference_t reference, candidate_t candidate) {
  bool ok = true;
  for (auto it = reference.begin(); it != reference.end(); it++) {
    uint64_t key = it->first;
    uint64_t ref_value = it->second;
    uint64_t can_value_trans = 0;
    uint64_t can_value_nontrans = 0;
    bool found_trans, found_nontrans;
    TRANSACTION {
      found_trans = candidate.trans_get(key, can_value_trans);
    } RETRY(false);

    found_nontrans = candidate.get(key, can_value_nontrans);

    if (!found_trans) {
      printf("\nERROR: key %lu with value %lu not found in candidate (transactional get)\n",
          key, ref_value);
      ok = false;
    } else if (ref_value != can_value_trans) {
      printf("\nERROR: key %lu with value %lu has value %lu in candidate (transactional get\n)",
          key, ref_value, can_value_trans);
      ok = false;
    }

    if (!found_nontrans) {
      printf("\nERROR: key %lu with value %lu not found in candidate (nontransactional get)\n",
         key,ref_value);
      ok = false;
    } else if (ref_value != can_value_nontrans) {
      printf("\nERROR: key %lu with value %lu has value %lu in candidate (nontransactional get)\n",
         key, ref_value, can_value_nontrans);
      ok = false;
    }
  }

  // TODO: check for extra keys in candidate (need range queries to do this / make it significant)

  return ok;
}

void print_result(bool ok) {
  printf(ok ? "PASS\n" : "FAIL\n");
}


// random puts in separate transactions
// verifies basic get/put behavior without concurrent transactions
void serial_random_puts() {
  bool ok = true;
  reference_t ref;
  candidate_t can;

  std::mt19937_64 gen(GLOBAL_SEED);
  std::uniform_int_distribution<uint64_t> rand;

  printf("\trandom puts ");
  reference_t writes;
  for (int i = 0; i < 1024; i++) {
    writes.clear();
    TRANSACTION {
      for (int j = 0; j < 64; j++) {
        uint64_t key = rand(gen) % 8192; // restrict range so there are some collisions
        uint64_t val = rand(gen);
        can.trans_put(key, val);
        writes[key] = val;
        ref[key] = val;
      }
      // make sure we can read our writes in the transaction
      for (auto it = writes.begin(); it != writes.end(); it++) {
        uint64_t val = 0;
        if (!can.trans_get(it->first, val)) {
          ok = false;
          printf("ERROR: write not found\n");
        }
        if (val != it->second) {
          ok = false;
          printf("ERROR: write value incorrect, got %lu expected %lu\n", val, it->second);
        }
      }
    } RETRY(false);
    if (!check_tree(ref, can)) {
      ok = false;
    }
  }

  print_result(ok);
}

// get-get
// get-put
// get-remove
// put-put
// put-remove
// remove-remove

// test abort from conflicting puts
void serial_conflicting_puts() {
  bool ok = true;
  candidate_t tree;
  Transaction t1;
  Transaction t2;

  printf("\tconflicting puts ");

  Transaction::threadid = 0;
  Sto::set_transaction(&t1);

  tree.trans_put(123, 456);

  Transaction::threadid = 1;
  Sto::set_transaction(&t2);

  bool aborted = false;
  try {
    tree.trans_put(123, 234);
  } catch (Transaction::Abort e) {
    aborted = true;
  }

  Transaction::threadid = 0;
  Sto::set_transaction(&t1);
  if (!t1.try_commit()) {
    ok = false;
    printf("\nERROR: earlier put did not commit\n");
  }

  Transaction::threadid = 1;
  Sto::set_transaction(&t2);
  if (!aborted) {
    printf("\nERROR: later put did not abort\n");
  }

  print_result(ok);
}

// test abort from conflicting gets and put, where the key already exists
void serial_conflicting_gets_put() {
  bool ok = true;
  candidate_t tree;
  tree.put(123, 456);

  Transaction t1;
  Transaction t2;

  printf("\tconflicting gets and put (key exists) ");

  Transaction::threadid = 0;
  Sto::set_transaction(&t1);

  // get from transaction 1
  uint64_t read_val;
  bool found = tree.trans_get(123, read_val);
  if (!found) {
    ok = false;
    printf("\nERROR: value not found for first get\n");
  }
  if (read_val != 456) {
    ok = false;
    printf("\nERROR: incorrect value %ld for first get\n", read_val);
  }

  Transaction::threadid = 1;
  Sto::set_transaction(&t2);

  // put from transaction 2
  tree.trans_put(123, 234);
  if (!t2.try_commit()) {
    ok = false;
    printf("\nERROR: put transaction did not commit\n");
  }

  Transaction::threadid = 0;
  Sto::set_transaction(&t1);

  // get from transaction 1 - should abort
  bool aborted = false;
  try {
    found = tree.trans_get(123, read_val);
  } catch (Transaction::Abort e) {
    aborted = true;
  }

  if (!aborted) {
    printf("\nERROR: second get did not abort\n");
  }

  print_result(ok);
}

void serial_tests() {
  printf("Running serial tests\n");
  serial_random_puts();
  serial_conflicting_puts();
  serial_conflicting_gets_put();
}

void parallel_tests(int nthreads) {
  /*
  vector<pthread_t> tids(nthreads);
  TesterPair<T> testers[N_THREADS];
  for (int i = 0; i < N_THREADS; ++i) {
      testers[i].t = queue;
      testers[i].me = i;
      if (parallel)
          pthread_create(&tids[i], NULL, runFunc<T>, &testers[i]);
      else
          pthread_create(&tids[i], NULL, runConcFunc, &testers[i]);
  }
  pthread_t advancer;
  pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
  pthread_detach(advancer);

  for (int i = 0; i < N_THREADS; ++i) {
      pthread_join(tids[i], NULL);
  }
  */
}

void run_parallel() {

}

void performance_tests() {

}
int main() {
  serial_tests();
  parallel_tests(4);
  performance_tests();
}
