#include <string>
#include <iostream>
#include <vector>
#include <random>
#include <map>
#include <iostream>
#include <chrono>
#include <cassert>
#include <cstring>

#include "Transaction.hh"
#include "RadixTree.hh"

#define GLOBAL_SEED 11
#define N_THREADS 8

typedef std::map<uint64_t, uint64_t> reference_t;
typedef RadixTree<uint64_t, uint64_t> candidate_t;

bool check_tree(reference_t &reference, candidate_t &candidate) {
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


// random puts and removes in separate transactions
// verifies basic get/put behavior without concurrent transactions
void serial_random() {
  bool ok = true;
  reference_t ref;
  candidate_t can;

  std::mt19937_64 gen(GLOBAL_SEED);
  std::uniform_int_distribution<uint64_t> rand;

  printf("\trandom puts ");
  reference_t puts;
  reference_t removes;
  for (int i = 0; i < 1024; i++) {
    puts.clear();
    TRANSACTION {
      for (int j = 0; j < 64; j++) {
        uint64_t key = rand(gen) % 8192; // restrict range so there are some collisions
        uint64_t remove = rand(gen) % 2; // 0 = put, 1 = remove
        uint64_t val = rand(gen);
        if (remove) {
          can.trans_remove(key);
          puts.erase(key);
          removes[key] = 1;
          ref.erase(key);
        } else {
          can.trans_put(key, val);
          puts[key] = val;
          removes.erase(key);
          ref[key] = val;
        }
      }
      // make sure we can read our removes and puts in the transaction
      for (auto it = puts.begin(); it != puts.end(); it++) {
        uint64_t val = 0;
        if (!can.trans_get(it->first, val)) {
          ok = false;
          printf("\nERROR: put not found\n");
        }
        if (val != it->second) {
          ok = false;
          printf("\nERROR: put value incorrect, got %lu expected %lu\n", val, it->second);
        }
      }
      for (auto it = removes.begin(); it != removes.end(); it++) {
        uint64_t val = 0;
        if (can.trans_get(it->first, val)) {
          ok = false;
          printf("\nERROR: removed value found\n");
        }
      }
    } RETRY(false);
    if (!check_tree(ref, can)) {
      ok = false;
    }
  }

  print_result(ok);
}

// test conflicting gets where the key exists
// both should succeed
void serial_gets() {
  bool ok = true;
  candidate_t tree;
  tree.put(123, 456);

  Transaction t1;
  Transaction t2;

  printf("\tconflicting gets ");

  bool found;
  uint64_t val;

  // get from transaction 1
  Transaction::threadid = 0;
  Sto::set_transaction(&t1);
  found = tree.trans_get(123, val);
  if (!found) {
    ok = false;
    printf("\nERROR: value not found\n");
  }
  if (val != 456) {
    ok = false;
    printf("\nERROR: expected value %d from transaction 1, got %lu\n", 456, val);
  }

  // get from transaction 2
  Transaction::threadid = 1;
  Sto::set_transaction(&t2);
  found = tree.trans_get(123, val);
  if (!found) {
    ok = false;
    printf("\nERROR: value not found\n");
  }
  if (val != 456) {
    ok = false;
    printf("\nERROR: expected value %d from transaction 2, got %lu\n", 456, val);
  }

  if (!t1.try_commit()) {
    ok = false;
    printf("\nERROR: transaction 1 did not commit\n");
  }
  if (!t2.try_commit()) {
    ok = false;
    printf("\nERROR: transaction 2 did not commit\n");
  }

  print_result(ok);
}

// test conflicting puts - should not abort
void serial_puts() {
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

  tree.trans_put(123, 234);

  Transaction::threadid = 0;
  Sto::set_transaction(&t1);
  if (!t1.try_commit()) {
    ok = false;
    printf("\nERROR: earlier put did not commit\n");
  }

  Transaction::threadid = 1;
  Sto::set_transaction(&t2);
  if (!t2.try_commit()) {
    ok = false;
    printf("\nERROR: later put did not commit\n");
  }

  uint64_t val = 0;
  if (!tree.get(123, val)) {
    ok = false;
    printf("\nERROR: value not found\n");
  }
  if (val != 234) {
    ok = false;
    printf("\nERROR: expected value %d from later commit, got %lu\n", 234, val);
  }

  print_result(ok);
}

// test conflicting get and put where the key doesn't exist
// the get after the put should abort
void serial_get_put_no_key() {
  bool ok = true;
  candidate_t tree;

  Transaction t1;
  Transaction t2;

  printf("\tconflicting gets and put (key doesn't exist) ");

  // put from transaction 2
  Transaction::threadid = 1;
  Sto::set_transaction(&t2);
  tree.trans_put(123, 234);

  // get from transaction 1 before put commits - should not abort
  Transaction::threadid = 0;
  Sto::set_transaction(&t1);
  uint64_t read_val;
  bool found = tree.trans_get(123, read_val);
  if (found) {
    ok = false;
    printf("\nERROR: nonexistent value found\n");
  }

  // commit from transaction 2
  Transaction::threadid = 1;
  Sto::set_transaction(&t2);
  if (!t2.try_commit()) {
    ok = false;
    printf("\nERROR: put transaction did not commit\n");
  }

  // get from transaction 1 after put commits - should abort
  Transaction::threadid = 0;
  Sto::set_transaction(&t1);
  bool aborted = false;
  try {
    found = tree.trans_get(123, read_val);
  } catch (Transaction::Abort e) {
    aborted = true;
  }

  if (!aborted) {
    printf("\nERROR: get after put committed did not abort\n");
  }

  print_result(ok);
}

// test conflicting get and put where the key already exists
// the get after the put should abort
void serial_get_put_key_exists() {
  bool ok = true;
  candidate_t tree;
  tree.put(123, 456);

  Transaction t1;
  Transaction t2;

  printf("\tconflicting gets and put (key exists) ");

  // put from transaction 2
  Transaction::threadid = 1;
  Sto::set_transaction(&t2);
  tree.trans_put(123, 234);

  // get from transaction 1 before put commits - should not abort
  Transaction::threadid = 0;
  Sto::set_transaction(&t1);
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

  // commit from transaction 2
  Transaction::threadid = 1;
  Sto::set_transaction(&t2);
  if (!t2.try_commit()) {
    ok = false;
    printf("\nERROR: put transaction did not commit\n");
  }

  // get from transaction 1 after put commits - should abort
  Transaction::threadid = 0;
  Sto::set_transaction(&t1);
  bool aborted = false;
  try {
    found = tree.trans_get(123, read_val);
  } catch (Transaction::Abort e) {
    aborted = true;
  }

  if (!aborted) {
    printf("\nERROR: get after put committed did not abort\n");
  }

  print_result(ok);
}

// test conflicting gets and removes when the key does not exist
// both should succeed
void serial_get_remove_no_key() {
  bool ok = true;
  candidate_t tree;

  Transaction t1;
  Transaction t2;

  printf("\tconflicting get and remove (key doesn't exist) ");

  // remove from t2
  Transaction::threadid = 1;
  Sto::set_transaction(&t2);
  tree.trans_remove(123);

  // get from transaction 1 before remove commits - should not abort
  Transaction::threadid = 0;
  Sto::set_transaction(&t1);
  uint64_t read_val;
  bool found = tree.trans_get(123, read_val);
  if (found) {
    ok = false;
    printf("\nERROR: value found in first get\n");
  }

  // commit from transaction 2
  Transaction::threadid = 1;
  Sto::set_transaction(&t2);
  if (!t2.try_commit()) {
    ok = false;
    printf("\nERROR: remove transaction did not commit\n");
  }

  // get from transaction 1 after remove commits - should not abort
  Transaction::threadid = 0;
  Sto::set_transaction(&t1);
  found = tree.trans_get(123, read_val);
  if (found) {
    ok = false;
    printf("\nERROR: value found in second get\n");
  }
  if (!t1.try_commit()) {
    ok = false;
    printf("\nERROR: get transaction did not commit\n");
  }

  print_result(ok);
}

// test conflicting gets and removes when the key already exists
// the get after the remove should abort
void serial_get_remove_key_exists() {
  bool ok = true;
  candidate_t tree;
  tree.put(123, 456);

  Transaction t1;
  Transaction t2;

  printf("\tconflicting get and remove (key exists) ");

  // remove from t2
  Transaction::threadid = 1;
  Sto::set_transaction(&t2);
  tree.trans_remove(123);

  // get from transaction 1 before remove commits - should not abort
  Transaction::threadid = 0;
  Sto::set_transaction(&t1);
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

  // commit from transaction 2
  Transaction::threadid = 1;
  Sto::set_transaction(&t2);
  if (!t2.try_commit()) {
    ok = false;
    printf("\nERROR: remove transaction did not commit\n");
  }

  // get from transaction 1 after remove commits - should abort
  Transaction::threadid = 0;
  Sto::set_transaction(&t1);
  bool aborted = false;
  try {
    found = tree.trans_get(123, read_val);
  } catch (Transaction::Abort e) {
    aborted = true;
  }

  if (!aborted) {
    ok = false;
    printf("\nERROR: get after remove committed did not abort\n");
  }

  uint64_t val = 0;
  if (tree.get(123, val)) {
    ok = false;
    printf("\nERROR: removed value found\n");
  }

  print_result(ok);
}

// test conflicting put then remove when the key doesn't exist
// the remove should abort
void serial_put_then_remove_no_key() {
  bool ok = true;
  candidate_t tree;

  Transaction t1;
  Transaction t2;

  printf("\tconflicting put then remove (key doesn't exist) ");

  // put with remove in between
  // put from t1
  Transaction::threadid = 0;
  Sto::set_transaction(&t1);
  tree.trans_put(123, 456);

  // remove from transaction 2 before put commits - should not abort
  Transaction::threadid = 1;
  Sto::set_transaction(&t2);
  tree.trans_remove(123);
  if (!t2.try_commit()) {
    ok = false;
    printf("\nERROR: remove transaction before put did not commit\n");
  }

  // put transaction should commit too
  if (!t1.try_commit()) {
    ok = false;
    printf("\nERROR: put transaction did not commit\n");
  }

  uint64_t read_val;
  if (!tree.get(123, read_val)) {
    ok = false;
    printf("\nERROR: value not found\n");
  }
  if (read_val != 456) {
    printf("\nERROR: incorrect value %lu after put\n", read_val);
  }

  print_result(ok);
}

// test conflicting put then remove when the key exists
// both should succeed
void serial_put_then_remove_key_exists() {
  bool ok = true;
  candidate_t tree;

  Transaction t1;
  Transaction t2;

  printf("\tconflicting put then remove (key exists) ");

  tree.put(123, 789);

  // put with remove in between
  // put from t1
  Transaction::threadid = 0;
  Sto::set_transaction(&t1);
  tree.trans_put(123, 456);

  // remove from transaction 2 before put commits - should not abort
  Transaction::threadid = 1;
  Sto::set_transaction(&t2);
  tree.trans_remove(123);
  if (!t2.try_commit()) {
    ok = false;
    printf("\nERROR: remove transaction before put did not commit\n");
  }

  // put transaction should commit too
  if (!t1.try_commit()) {
    ok = false;
    printf("\nERROR: put transaction did not commit\n");
  }

  uint64_t read_val;
  if (!tree.get(123, read_val)) {
    ok = false;
    printf("\nERROR: value not found\n");
  }
  if (read_val != 456) {
    printf("\nERROR: incorrect value %lu after put\n", read_val);
  }

  print_result(ok);
}

// test conflicting remove then put when the key doesn't exist
// both should succeed
void serial_remove_then_put_no_key() {
  bool ok = true;
  candidate_t tree;

  Transaction t1;
  Transaction t2;

  printf("\tconflicting remove then put (key doesn't exist) ");

  // remove with put in between
  Transaction::threadid = 0;
  Sto::set_transaction(&t1);
  tree.trans_remove(123);

  // put from transaction 2 before remove commits - should not abort
  Transaction::threadid = 1;
  Sto::set_transaction(&t2);
  tree.trans_put(123, 789);
  if (!t2.try_commit()) {
    ok = false;
    printf("\nERROR: put transaction before remove did not commit\n");
  }

  // remove transaction should not commit (yes, this is a bit weird)
  if (t1.try_commit()) {
    ok = false;
    printf("\nERROR: remove transaction did not abort\n");
  }

  uint64_t read_val = 0;
  if (!tree.get(123, read_val)) {
    ok = false;
    printf("\nERROR: value not found\n");
  }
  if (read_val != 789) {
    ok = false;
    printf("\nERROR: incorrect value %lu after put\n", read_val);
  }

  print_result(ok);
}


// test conflicting remove then put when the key exists
// both should succeed
void serial_remove_then_put_key_exists() {
  bool ok = true;
  candidate_t tree;

  Transaction t1;
  Transaction t2;

  printf("\tconflicting remove then put (key exists) ");

  tree.put(123, 456);

  // remove with put in between
  Transaction::threadid = 0;
  Sto::set_transaction(&t1);
  tree.trans_remove(123);

  // put from transaction 2 before remove commits - should not abort
  Transaction::threadid = 1;
  Sto::set_transaction(&t2);
  tree.trans_put(123, 789);
  if (!t2.try_commit()) {
    ok = false;
    printf("\nERROR: put transaction before remove did not commit\n");
  }

  // remove transaction should commit too
  if (!t1.try_commit()) {
    ok = false;
    printf("\nERROR: put transaction did not commit\n");
  }

  uint64_t read_val;
  if (tree.get(123, read_val)) {
    ok = false;
    printf("\nERROR: removed value found\n");
  }

  print_result(ok);
}

// test conflicting removes - should not abort
void serial_removes() {
  bool ok = true;
  candidate_t tree;
  Transaction t1;
  Transaction t2;
  tree.put(123, 456);

  printf("\tconflicting removes ");

  Transaction::threadid = 0;
  Sto::set_transaction(&t1);
  tree.trans_remove(123);

  Transaction::threadid = 1;
  Sto::set_transaction(&t2);
  tree.trans_remove(123);

  Transaction::threadid = 0;
  Sto::set_transaction(&t1);
  if (!t1.try_commit()) {
    ok = false;
    printf("\nERROR: earlier remove did not commit\n");
  }

  Transaction::threadid = 1;
  Sto::set_transaction(&t2);
  if (!t2.try_commit()) {
    ok = false;
    printf("\nERROR: later remove did not commit\n");
  }

  uint64_t val = 0;
  if (tree.get(123, val)) {
    ok = false;
    printf("\nERROR: removed value found\n");
  }

  print_result(ok);
}

void serial_cleanup() {
  Sto::clear_transaction();
}

void serial_tests() {
  printf("Running serial tests\n");
  serial_random();
  serial_gets();
  serial_puts();
  serial_get_put_no_key();
  serial_get_put_key_exists();
  serial_get_remove_no_key();
  serial_get_remove_key_exists();
  serial_put_then_remove_no_key();
  serial_put_then_remove_key_exists();
  serial_remove_then_put_no_key();
  serial_remove_then_put_key_exists();
  serial_removes();
  serial_cleanup();
}

struct perf_config {
  uint64_t ntrans;
  uint64_t nkeys;
  uint64_t sparseness;
  uint64_t txn_nops;
  double prob_write;

  uint64_t aborts;
  candidate_t *tree;
};

void perf_init(void *data) {
  auto c = static_cast<perf_config *>(data);
  c->tree = new candidate_t();

  std::mt19937_64 gen(GLOBAL_SEED);
  std::uniform_int_distribution<uint64_t> rand;
  for (uint64_t i = 0; i < c->nkeys; i++) {
    uint64_t key = c->sparseness * i;
    uint64_t val = rand(gen);
    c->tree->put(key, val);
  }
}

void *perf_random_gets(void *data) {
  auto c = static_cast<perf_config *>(data);
  auto tree = c->tree;

  std::mt19937_64 gen(GLOBAL_SEED);
  std::uniform_int_distribution<uint64_t> rand;
  std::uniform_real_distribution<double> randf(0, 1);
  uint64_t sum = 0, misses = 0;
  for (uint64_t i = 0; i < c->ntrans; i++) {
    TRANSACTION {
      for (uint64_t j = 0; j < c->txn_nops; j++) {
        uint64_t key = rand(gen) % (c->sparseness * c->nkeys);
        uint64_t val;
        if (randf(gen) < c->prob_write) {
          val = rand(gen);
          tree->trans_put(key, val);
        } else {
          if (tree->trans_get(key, val)) {
            misses++;
          }
          sum += val;
        }
      }
    } RETRY(true);
  }
  return nullptr;
}

void perf_cleanup(void *data) {
  auto c = static_cast<perf_config *>(data);
  delete c->tree;
}

void perf_run(perf_config *c, void (*init)(void *), void *(*func)(void *), void (*cleanup)(void *)) {
  printf("\tntrans: %lu, nkeys: %lu, sparseness: %lu, txn_nops: %lu, prob_write: %f\n",
      c->ntrans, c->nkeys, c->sparseness, c->txn_nops, c->prob_write);

  // serial
  perf_init(c);
  auto s_start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < N_THREADS; i++) {
    func(c);
  }
  auto s_end = std::chrono::high_resolution_clock::now();
  perf_cleanup(c);
  auto s_time = std::chrono::duration_cast<std::chrono::microseconds>(s_end - s_start);

  // parallel
  perf_init(c);
  auto p_start = std::chrono::high_resolution_clock::now();
  pthread_t threads[N_THREADS];
  for (int i = 0; i < N_THREADS; i++) {
    pthread_create(&threads[i], NULL, func, c);
  }
  for (int i = 0; i < N_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }
  auto p_end = std::chrono::high_resolution_clock::now();
  perf_cleanup(c);
  auto p_time = std::chrono::duration_cast<std::chrono::microseconds>(p_end - p_start);
  auto n_ops = c->txn_nops * c->ntrans;
  printf("\tserial: %ld us (%f ops/s), parallel: %ld us (%f ops/s, %fx speedup)\n\n",
      s_time.count(), (double) 1000000 * n_ops / s_time.count(),
      p_time.count(), (double) 1000000 * n_ops / p_time.count(),
      (double) s_time.count()/p_time.count());
}

void performance_tests() {
  printf("Running performance tests\n");

  auto c = new perf_config();
  c->ntrans = 100000;
  c->nkeys = (1 << 20);
  c->sparseness = 1;
  c->txn_nops = 5;
  c->prob_write = 0.0;
  perf_run(c, &perf_init, &perf_random_gets, &perf_cleanup);

  c->prob_write = 0.25;
  perf_run(c, &perf_init, &perf_random_gets, &perf_cleanup);

  c->prob_write = 0.5;
  perf_run(c, &perf_init, &perf_random_gets, &perf_cleanup);

  c->prob_write = 0.75;
  perf_run(c, &perf_init, &perf_random_gets, &perf_cleanup);
  delete c;
}

int main(int argc, char **argv) {
  bool run_serial = false;
  bool run_performance = false;
  if (argc < 2) {
    run_serial = true;
    run_performance = true;
  } else if (!strcmp("serial_test", argv[1])) {
    run_serial = true;
  } else if (!strcmp("performance", argv[1])) {
    run_performance = true;
  } else {
    printf("usage: ./radixtree [serial_test | performance]\n");
    return -1;
  }

  if (run_serial)
    serial_tests();
  if (run_performance)
    performance_tests();
}
