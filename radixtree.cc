#include <string>
#include <iostream>
#include <vector>
#include <random>
#include <map>
#include <iostream>
#include <cassert>
#include <cstring>

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

  // get from transaction 1 after remove commits, should not be found
  // XXX: this does not provide opacity
  Transaction::threadid = 0;
  Sto::set_transaction(&t1);
  found = tree.trans_get(123, read_val);

  if (found) {
    ok = false;
    printf("\nERROR: value found for get after remove committed\n");
  }

  if (t1.try_commit()) {
    ok = false;
    printf("\nERROR: get transaction did not fail\n");
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

  // put transaction should not commit
  if (t1.try_commit()) {
    ok = false;
    printf("\nERROR: put transaction after remove committed\n");
  }

  uint64_t read_val;
  if (tree.get(123, read_val)) {
    ok = false;
    printf("\nERROR: removed value found\n");
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

  // put transaction should not commit
  if (t1.try_commit()) {
    ok = false;
    printf("\nERROR: put transaction after remove committed\n");
  }

  uint64_t read_val;
  if (tree.get(123, read_val)) {
    ok = false;
    printf("\nERROR: removed value found\n");
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
}

void performance_tests() {
  printf("Running performance tests\n");
}

int main(int argc, char **argv) {
  bool run_serial = false;
  bool run_performance = false;
  if (argc < 2) {
    run_serial = true;
    run_performance = true;
  } else if (!strcmp("serial", argv[1])) {
    run_serial = true;
  } else if (!strcmp("performance", argv[1])) {
    run_performance = true;
  } else {
    printf("usage: ./radixtree [serial | performance]\n");
    return -1;
  }

  if (run_serial)
    serial_tests();
  if (run_performance)
    performance_tests();
}
