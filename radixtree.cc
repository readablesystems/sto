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
      fprintf(stderr, "\nERROR: key %lu with value %lu not found in candidate (transactional get)\n",
          key, ref_value);
      ok = false;
    } else if (ref_value != can_value_trans) {
      fprintf(stderr, "\nERROR: key %lu with value %lu has value %lu in candidate (transactional get\n)",
          key, ref_value, can_value_trans);
      ok = false;
    }

    if (!found_nontrans) {
      fprintf(stderr, "\nERROR: key %lu with value %lu not found in candidate (nontransactional get)\n",
         key,ref_value);
      ok = false;
    } else if (ref_value != can_value_nontrans) {
      fprintf(stderr, "\nERROR: key %lu with value %lu has value %lu in candidate (nontransactional get)\n",
         key, ref_value, can_value_nontrans);
      ok = false;
    }
  }

  // TODO: check for extra keys in candidate (need range queries to do this / make it significant)

  return ok;
}

void print_result(bool ok) {
  fprintf(stderr, ok ? "PASS\n" : "FAIL\n");
}


// random puts and removes in separate transactions
// verifies basic get/put behavior without concurrent transactions
void serial_random(bool opacity) {
  bool ok = true;
  reference_t ref;
  candidate_t can(opacity);

  std::mt19937_64 gen(GLOBAL_SEED);
  std::uniform_int_distribution<uint64_t> rand;

  fprintf(stderr, "\trandom puts ");
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
          fprintf(stderr, "\nERROR: put not found\n");
        }
        if (val != it->second) {
          ok = false;
          fprintf(stderr, "\nERROR: put value incorrect, got %lu expected %lu\n", val, it->second);
        }
      }
      for (auto it = removes.begin(); it != removes.end(); it++) {
        uint64_t val = 0;
        if (can.trans_get(it->first, val)) {
          ok = false;
          fprintf(stderr, "\nERROR: removed value found\n");
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
void serial_gets(bool opacity) {
  bool ok = true;
  candidate_t tree(opacity);
  tree.put(123, 456);

  Transaction t1;
  Transaction t2;

  fprintf(stderr, "\tconflicting gets ");

  bool found;
  uint64_t val;

  // get from transaction 1
  Transaction::threadid = 0;
  Sto::set_transaction(&t1);
  found = tree.trans_get(123, val);
  if (!found) {
    ok = false;
    fprintf(stderr, "\nERROR: value not found\n");
  }
  if (val != 456) {
    ok = false;
    fprintf(stderr, "\nERROR: expected value %d from transaction 1, got %lu\n", 456, val);
  }

  // get from transaction 2
  Transaction::threadid = 1;
  Sto::set_transaction(&t2);
  found = tree.trans_get(123, val);
  if (!found) {
    ok = false;
    fprintf(stderr, "\nERROR: value not found\n");
  }
  if (val != 456) {
    ok = false;
    fprintf(stderr, "\nERROR: expected value %d from transaction 2, got %lu\n", 456, val);
  }

  if (!t1.try_commit()) {
    ok = false;
    fprintf(stderr, "\nERROR: transaction 1 did not commit\n");
  }
  if (!t2.try_commit()) {
    ok = false;
    fprintf(stderr, "\nERROR: transaction 2 did not commit\n");
  }

  print_result(ok);
}

// test conflicting puts - should not abort
void serial_puts(bool opacity) {
  bool ok = true;
  candidate_t tree(opacity);
  Transaction t1;
  Transaction t2;

  fprintf(stderr, "\tconflicting puts ");

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
    fprintf(stderr, "\nERROR: earlier put did not commit\n");
  }

  Transaction::threadid = 1;
  Sto::set_transaction(&t2);
  if (!t2.try_commit()) {
    ok = false;
    fprintf(stderr, "\nERROR: later put did not commit\n");
  }

  uint64_t val = 0;
  if (!tree.get(123, val)) {
    ok = false;
    fprintf(stderr, "\nERROR: value not found\n");
  }
  if (val != 234) {
    ok = false;
    fprintf(stderr, "\nERROR: expected value %d from later commit, got %lu\n", 234, val);
  }

  print_result(ok);
}

// test conflicting get and put where the key doesn't exist
// the get after the put should abort
void serial_get_put_no_key(bool opacity) {
  bool ok = true;
  candidate_t tree(opacity);

  Transaction t1;
  Transaction t2;

  fprintf(stderr, "\tconflicting gets and put (key doesn't exist) ");

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
    fprintf(stderr, "\nERROR: nonexistent value found\n");
  }

  // commit from transaction 2
  Transaction::threadid = 1;
  Sto::set_transaction(&t2);
  if (!t2.try_commit()) {
    ok = false;
    fprintf(stderr, "\nERROR: put transaction did not commit\n");
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

  if (tree.has_opacity()) {
    if (!aborted) {
      ok = false;
      fprintf(stderr, "\nERROR: get after put committed did not abort\n");
    }
  } else {
    if (t1.try_commit()) {
      ok = false;
      fprintf(stderr, "\nERROR: get transaction did not abort\n");
    }
  }

  print_result(ok);
}

// test conflicting get and put where the key already exists
// the get after the put should abort
void serial_get_put_key_exists(bool opacity) {
  bool ok = true;
  candidate_t tree(opacity);
  tree.put(123, 456);

  Transaction t1;
  Transaction t2;

  fprintf(stderr, "\tconflicting gets and put (key exists) ");

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
    fprintf(stderr, "\nERROR: value not found for first get\n");
  }
  if (read_val != 456) {
    ok = false;
    fprintf(stderr, "\nERROR: incorrect value %ld for first get\n", read_val);
  }

  // commit from transaction 2
  Transaction::threadid = 1;
  Sto::set_transaction(&t2);
  if (!t2.try_commit()) {
    ok = false;
    fprintf(stderr, "\nERROR: put transaction did not commit\n");
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

  if (tree.has_opacity()) {
    if (!aborted) {
      ok = false;
      fprintf(stderr, "\nERROR: get after put committed did not abort\n");
    }
  } else {
    if (t1.try_commit()) {
      ok = false;
      fprintf(stderr, "\nERROR: get transaction did not abort\n");
    }
  }

  print_result(ok);
}

// test conflicting gets and removes when the key does not exist
// both should succeed
void serial_get_remove_no_key(bool opacity) {
  bool ok = true;
  candidate_t tree(opacity);

  Transaction t1;
  Transaction t2;

  fprintf(stderr, "\tconflicting get and remove (key doesn't exist) ");

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
    fprintf(stderr, "\nERROR: value found in first get\n");
  }

  // commit from transaction 2
  Transaction::threadid = 1;
  Sto::set_transaction(&t2);
  if (!t2.try_commit()) {
    ok = false;
    fprintf(stderr, "\nERROR: remove transaction did not commit\n");
  }

  // get from transaction 1 after remove commits - should not abort
  Transaction::threadid = 0;
  Sto::set_transaction(&t1);
  found = tree.trans_get(123, read_val);
  if (found) {
    ok = false;
    fprintf(stderr, "\nERROR: value found in second get\n");
  }
  if (!t1.try_commit()) {
    ok = false;
    fprintf(stderr, "\nERROR: get transaction did not commit\n");
  }

  print_result(ok);
}

// test conflicting gets and removes when the key already exists
// the get after the remove should abort
void serial_get_remove_key_exists(bool opacity) {
  bool ok = true;
  candidate_t tree(opacity);
  tree.put(123, 456);

  Transaction t1;
  Transaction t2;

  fprintf(stderr, "\tconflicting get and remove (key exists) ");

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
    fprintf(stderr, "\nERROR: value not found for first get\n");
  }
  if (read_val != 456) {
    ok = false;
    fprintf(stderr, "\nERROR: incorrect value %ld for first get\n", read_val);
  }

  // commit from transaction 2
  Transaction::threadid = 1;
  Sto::set_transaction(&t2);
  if (!t2.try_commit()) {
    ok = false;
    fprintf(stderr, "\nERROR: remove transaction did not commit\n");
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

  if (tree.has_opacity()) {
    if (!aborted) {
      ok = false;
      fprintf(stderr, "\nERROR: get after put committed did not abort\n");
    }
  } else {
    if (t1.try_commit()) {
      ok = false;
      fprintf(stderr, "\nERROR: get transaction did not abort\n");
    }
  }


  uint64_t val = 0;
  if (tree.get(123, val)) {
    ok = false;
    fprintf(stderr, "\nERROR: removed value found\n");
  }

  print_result(ok);
}

// test conflicting put then remove when the key doesn't exist
// the remove should abort
void serial_put_then_remove_no_key(bool opacity) {
  bool ok = true;
  candidate_t tree(opacity);

  Transaction t1;
  Transaction t2;

  fprintf(stderr, "\tconflicting put then remove (key doesn't exist) ");

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
    fprintf(stderr, "\nERROR: remove transaction before put did not commit\n");
  }

  // put transaction should commit too
  if (!t1.try_commit()) {
    ok = false;
    fprintf(stderr, "\nERROR: put transaction did not commit\n");
  }

  uint64_t read_val;
  if (!tree.get(123, read_val)) {
    ok = false;
    fprintf(stderr, "\nERROR: value not found\n");
  }
  if (read_val != 456) {
    fprintf(stderr, "\nERROR: incorrect value %lu after put\n", read_val);
  }

  print_result(ok);
}

// test conflicting put then remove when the key exists
// both should succeed
void serial_put_then_remove_key_exists(bool opacity) {
  bool ok = true;
  candidate_t tree(opacity);

  Transaction t1;
  Transaction t2;

  fprintf(stderr, "\tconflicting put then remove (key exists) ");

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
    fprintf(stderr, "\nERROR: remove transaction before put did not commit\n");
  }

  // put transaction should commit too
  if (!t1.try_commit()) {
    ok = false;
    fprintf(stderr, "\nERROR: put transaction did not commit\n");
  }

  uint64_t read_val;
  if (!tree.get(123, read_val)) {
    ok = false;
    fprintf(stderr, "\nERROR: value not found\n");
  }
  if (read_val != 456) {
    fprintf(stderr, "\nERROR: incorrect value %lu after put\n", read_val);
  }

  print_result(ok);
}

// test conflicting remove then put when the key doesn't exist
// both should succeed
void serial_remove_then_put_no_key(bool opacity) {
  bool ok = true;
  candidate_t tree(opacity);

  Transaction t1;
  Transaction t2;

  fprintf(stderr, "\tconflicting remove then put (key doesn't exist) ");

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
    fprintf(stderr, "\nERROR: put transaction before remove did not commit\n");
  }

  // remove transaction should not commit (yes, this is a bit weird)
  if (t1.try_commit()) {
    ok = false;
    fprintf(stderr, "\nERROR: remove transaction did not abort\n");
  }

  uint64_t read_val = 0;
  if (!tree.get(123, read_val)) {
    ok = false;
    fprintf(stderr, "\nERROR: value not found\n");
  }
  if (read_val != 789) {
    ok = false;
    fprintf(stderr, "\nERROR: incorrect value %lu after put\n", read_val);
  }

  print_result(ok);
}


// test conflicting remove then put when the key exists
// both should succeed
void serial_remove_then_put_key_exists(bool opacity) {
  bool ok = true;
  candidate_t tree(opacity);

  Transaction t1;
  Transaction t2;

  fprintf(stderr, "\tconflicting remove then put (key exists) ");

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
    fprintf(stderr, "\nERROR: put transaction before remove did not commit\n");
  }

  // remove transaction should commit too
  if (!t1.try_commit()) {
    ok = false;
    fprintf(stderr, "\nERROR: put transaction did not commit\n");
  }

  uint64_t read_val;
  if (tree.get(123, read_val)) {
    ok = false;
    fprintf(stderr, "\nERROR: removed value found\n");
  }

  print_result(ok);
}

// test conflicting removes - should not abort
void serial_removes(bool opacity) {
  bool ok = true;
  candidate_t tree(opacity);
  Transaction t1;
  Transaction t2;
  tree.put(123, 456);

  fprintf(stderr, "\tconflicting removes ");

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
    fprintf(stderr, "\nERROR: earlier remove did not commit\n");
  }

  Transaction::threadid = 1;
  Sto::set_transaction(&t2);
  if (!t2.try_commit()) {
    ok = false;
    fprintf(stderr, "\nERROR: later remove did not commit\n");
  }

  uint64_t val = 0;
  if (tree.get(123, val)) {
    ok = false;
    fprintf(stderr, "\nERROR: removed value found\n");
  }

  print_result(ok);
}

void serial_cleanup() {
  Sto::clear_transaction();
}

void serial_tests(bool opacity) {
  fprintf(stderr, "Running serial tests (%s)\n", opacity ? "with opacity" : "without opacity");
  serial_random(opacity);
  serial_gets(opacity);
  serial_puts(opacity);
  serial_get_put_no_key(opacity);
  serial_get_put_key_exists(opacity);
  serial_get_remove_no_key(opacity);
  serial_get_remove_key_exists(opacity);
  serial_put_then_remove_no_key(opacity);
  serial_put_then_remove_key_exists(opacity);
  serial_remove_then_put_no_key(opacity);
  serial_remove_then_put_key_exists(opacity);
  serial_removes(opacity);
  serial_cleanup();
}

struct perf_config {
  int nthreads;
  int nruns;
  uint64_t ntrans;
  bool opacity;
  uint64_t tree_size;
  uint64_t sparseness;
  uint64_t txn_nops;
  double prob_write;

  uint64_t aborts;
  candidate_t *tree;
};

void perf_init(perf_config *c) {
  c->aborts = 0;

  // special case for sparseness == 1, skip reinitializing the tree
  if (c->sparseness == 1 && c->tree != nullptr) {
    return;
  }

  // put random values in the tree
  c->tree = new candidate_t(c->opacity);
  std::mt19937_64 gen(GLOBAL_SEED);
  std::uniform_int_distribution<uint64_t> rand;
  for (uint64_t i = 0; i < c->tree_size; i++) {
    uint64_t key = c->sparseness * i;
    uint64_t val = rand(gen);
    c->tree->put(key, val);
  }
}

void *perf_run(void *data) {
  auto c = static_cast<perf_config *>(data);
  auto tree = c->tree;

  std::mt19937_64 gen(GLOBAL_SEED);
  std::uniform_int_distribution<uint64_t> rand;
  std::uniform_real_distribution<double> randf(0, 1);
  uint64_t sum = 0, misses = 0;
  Transaction t;
  uint64_t aborts = 0;
  for (uint64_t i = 0; i < c->ntrans; i++) {
    while (true) {
      t.reset();
      Sto::set_transaction(&t);
      try {
        for (uint64_t j = 0; j < c->txn_nops; j++) {
          uint64_t key = rand(gen) % (c->sparseness * c->tree_size);
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
        t.commit();
        break;
      } catch (Transaction::Abort e) {
        aborts++;
      }
    }
  }
  __sync_fetch_and_add(&c->aborts, aborts);
  return nullptr;
}

void *perf_run_no_txn(void *data) {
  auto c = static_cast<perf_config *>(data);
  auto tree = c->tree;

  std::mt19937_64 gen(GLOBAL_SEED);
  std::uniform_int_distribution<uint64_t> rand;
  std::uniform_real_distribution<double> randf(0, 1);
  uint64_t sum = 0, misses = 0;
  uint64_t aborts = 0;
  for (uint64_t i = 0; i < c->ntrans; i++) {
    for (uint64_t j = 0; j < c->txn_nops; j++) {
      uint64_t key = rand(gen) % (c->sparseness * c->tree_size);
      uint64_t val;
      if (randf(gen) < c->prob_write) {
        val = rand(gen);
        tree->put(key, val);
      } else {
        if (tree->get(key, val)) {
          misses++;
        }
        sum += val;
      }
    }
  }
  __sync_fetch_and_add(&c->aborts, aborts);
  return nullptr;
}

void perf_cleanup(perf_config *c, bool full) {
  if (c->sparseness != 1 || full) {
    if (c->tree != nullptr) {
      delete c->tree;
      c->tree = nullptr;
    }
  }
}

void perf_print_header() {
  printf("nruns,ntrans,opacity,tree_size,sparseness,txn_nops,prob_write,"
      "nthreads,serial_us,serial_ops_s,parallel_us,parallel_ops_s,speedup,aborts\n");
}

void perf_print_data(perf_config *c, int nthreads, uint64_t ser_time, uint64_t par_time) {
  auto n_ops = c->nruns * c->txn_nops * c->ntrans;
  printf("%d,%lu,%d,%lu,%lu,%lu,%f,",
      c->nruns, c->ntrans, c->opacity, c->tree_size, c->sparseness, c->txn_nops, c->prob_write);
  printf("%d,%lu,%f,%ld,%f,%f,%lu\n",
      nthreads,
      ser_time, (double) 1000000 * n_ops / ser_time,
      par_time, (double) 1000000 * n_ops / par_time,
      (double) ser_time / par_time,
      c->aborts);
  fflush(stdout);
}

void perf_run(perf_config *c) {
  // baseline - serial without transactions
  // We avoid taking O(nthreads^2) time here by measuring the serial time
  // for all the possible numbers of threads together.
  std::vector<uint64_t> ser_times(c->nthreads);
  for (int i = 0; i < c->nruns; i++) {
    perf_init(c);
    auto ser_start = std::chrono::high_resolution_clock::now();
    for (int j = 0; j < c->nthreads; j++) {
      perf_run_no_txn(static_cast<void *>(c));
      auto ser_end = std::chrono::high_resolution_clock::now();
      ser_times[j] += std::chrono::duration_cast<std::chrono::microseconds>(ser_end - ser_start).count();
    }
    perf_cleanup(c, false);
  }

  for (int nt = 1; nt <= c->nthreads; nt++) {
    // parallel
    uint64_t par_time = 0;
    for (int i = 0; i < c->nruns; i++) {
      perf_init(c);
      std::vector<pthread_t> threads(c->nthreads);
      auto p_start = std::chrono::high_resolution_clock::now();
      for (int i = 0; i < nt; i++) {
        pthread_create(&threads[i], NULL, perf_run, c);
      }
      for (int i = 0; i < nt; i++) {
        pthread_join(threads[i], NULL);
      }
      auto p_end = std::chrono::high_resolution_clock::now();
      perf_cleanup(c, false);

      par_time += std::chrono::duration_cast<std::chrono::microseconds>(p_end - p_start).count();
    }
    perf_print_data(c, nt, ser_times[nt-1], par_time);
  }

  perf_cleanup(c, true);
}

void performance_tests() {
  fprintf(stderr, "Running performance tests\n");

  perf_print_header();
  for (bool opacity: {false, true}) {
    for (int tree_size : {1 << 12, 1 << 17, 1 << 22}) {
      for (int sparseness : {1}) {
        for (int txn_nops : {1, 5, 20, 50}) {
          for (int prob_write : {0, 10, 20, 50, 80}) {
            perf_config c;
            c.tree = nullptr;
            c.ntrans = 20000;
            c.nruns = 5;
            c.opacity = opacity;
            c.tree_size = tree_size;
            c.sparseness = sparseness;
            c.txn_nops = txn_nops;
            c.prob_write = prob_write / 100.0;
            c.nthreads = 12;
            perf_run(&c);
          }
        }
      }
    }
  }
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
    fprintf(stderr, "usage: ./radixtree [serial_test | performance]\n");
    return -1;
  }

  if (run_serial) {
    serial_tests(true);
    serial_tests(false);
  }
  if (run_performance)
    performance_tests();
}
