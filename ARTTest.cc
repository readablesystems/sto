#include <cassert>
#include <cstdlib>
#include <iostream>
#include <pthread.h>
#include <unistd.h>

#include "ARTTree.hh"

struct insert_info {
  std::string key;
  int val;
  ARTTree<int>* tree;
};

struct update_info {
  std::string key;
  int val;
  ARTTree<int>* tree;
};

struct lookup_info {
  std::string key;
  int retval;
  bool success;
  ARTTree<int>* tree;
};

// Testing Strategy:
//
//////////////////////////////////////////////////////////////////////////////////
//
// insert(string key, V value)
// key length: 0, 1, > 1
// value: small size value, medium size value, large value
// record for key: existed before call, did not exist before call
//
// Will test every case (except for when key length is 0 or 1, which will only be test once)
// at least twice.
//
//////////////////////////////////////////////////////////////////////////////////
//
// lookup(string key, V& retval)
//  length of key: 0, 1, > 1 
//  character types in key: lowercase letters, uppercase letters, punctuation, symbols, \0 character
//  retval types: V variable, literal, null, non-V variable
//  record for key: existed before call, did not exist before call
//
//  Will test every case (except where key length is 0 or 1 and where reval type is literal, null,
//  or non-V variable, which will be tested only once) at least twice.
//
//////////////////////////////////////////////////////////////////////////////////
//
// update(string key, V value)
//  key length: 0, 1, > 1
//  value: small size value, medium size value, large value
//  record for key: existed before call, did not exist before call
//
// Will test every case (except where key length is 0 or 1, which will only be tested once) at
// least twice.
//
//////////////////////////////////////////////////////////////////////////////////
//
// remove(string key)
// key length: 0, 1 , > 1
// record for key: existed before call, did not exist before call
//
// Will test every case (except when key length is 0 or 1, which will be tested only once) at least
// twice.

//////////////////////////////////////////////////////////////////////////////////

static const std::string KEY = "";
static const std::string KEY2 = "T";
static const std::string KEY3 = "!threE.";
static const std::string KEY4 = "$fOUR,";
static const std::string KEY5 = "fivefivefivefivefivefive";
static const std::string KEY6 = ".s.i,xX%.";
static const std::string KEY7 = "seven";
static const std::string KEY8 = "e\0ight\0";
static const std::string KEY9 = "\0NINE";
static const std::string KEY10 = "HELLO";
static const std::string KEY11 = "HALLO";
static const std::string KEY12 = "HOLLO";
static const std::string KEY13 = "HILLO";
static const std::string KEY14 = "HULLO";
static const int VAL = 3;
static const int VAL2 = 40;
static const int VAL3 = 100;
static const int VAL4 = 150000000;
static const int VAL5 = -200000;
static const int VAL6 = 2147483646;
static const int VAL7 = 12345678;
static const int VAL8 = -2147483646;

// Tests for lookup
void test_length_zero_low_value_no_exist() {
  ARTTree<int> root = ARTTree<int>();
  root.insert(KEY, VAL);
  int val = 0;
  bool success = root.lookup(KEY, val);
  assert(success && val == VAL && "test failed");
}

void test_length_one_low_value_exists() {
  ARTTree<int> root = ARTTree<int>();
  root.insert(KEY2, VAL2);
  root.insert(KEY2, VAL3);
  int val = 0;
  bool success = root.lookup(KEY2, val);
  assert(success && val == VAL3 && "test failed");
}

void test_length_more_than_one_med_value_no_exist() {
  ARTTree<int> root = ARTTree<int>();
  root.insert(KEY3, VAL4);
  int val = 0;
  bool success = root.lookup(KEY3, val);
  assert(success && val == VAL4 && "test failed");
}

void test_length_more_than_one_large_value_exists() {
  ARTTree<int> root = ARTTree<int>();
  root.insert(KEY4, VAL5);
  root.insert(KEY4, VAL6);
  int val = 0;
  bool success = root.lookup(KEY4, val);
  assert(success && val == VAL6 && "test failed");
}

void test_length_more_than_one_med_value_no_exist2() {
  ARTTree<int> root = ARTTree<int>();
  root.insert(KEY5, VAL7);
  int val = 0;
  bool success = root.lookup(KEY5, val);
  assert(success && val == VAL7 && "test failed");
}

void test_length_more_than_one_large_value_exists2() {
  ARTTree<int> root = ARTTree<int>();
  root.insert(KEY6, VAL4);
  root.insert(KEY6, VAL8);
  int val = 0;
  bool success = root.lookup(KEY6, val);
  assert(success && val == VAL8 && "test failed");
}

// Tests for insert

void test_length_zero_v_variable_exists() {
  ARTTree<int> root = ARTTree<int>();
  root.insert(KEY, VAL);
  int val = 0;
  bool success = root.lookup(KEY, val);
  assert(success && val == VAL && "test failed"); // Change this if needed
}

void test_length_one_upper_v_variable_exists() {
  ARTTree<int> root = ARTTree<int>();
  root.insert(KEY2, VAL4);
  int val = 0;
  bool success = root.lookup(KEY2, val);
  assert(success && val == VAL4 && "test failed");
}

void test_length_more_than_one_low_upper_punc_v_variable_exists() {
  ARTTree<int> root = ARTTree<int>();
  root.insert(KEY3, VAL2);
  int val = 0;
  bool success = root.lookup(KEY3, val);
  assert(success && val == VAL2 && "test failed");
}

// void test_length_more_than_one_low_sym_upper_punc_null_exists() {
//   ARTNode256<int> root = ARTNode256<int>();
//   root.insert(KEY4, VAL5);
//   bool success = root.lookup(KEY2, NULL); // should fail here
// }

// void test_length_one_upper_literal_exists() {
//   ARTNode256<int> root = ARTNode256<int>();
//   root.insert(KEY2, VAL3);
//   bool success = root.lookup(KEY2, 10); // should fail here
//   assert(success && val == VAL3 && "test failed");
// }

// void test_length_more_than_one_low_non_v_literal_exists() {
//   ARTNode256<int> root = ARTNode256<int>();
//   root.insert(KEY7, VAL6);
//   char val = 'a';
//   bool success = root.lookup(KEY2, val); // should fail here
//   assert(success && val == VAL6 && "test failed");
// }

void test_length_more_than_one_punc_lower_sym_v_variable_no_exist() {
  ARTTree<int> root = ARTTree<int>();
  int val = 0;
  bool success = root.lookup(KEY6, val);
  assert(!success && val == 0 && "test failed");
}

void test_length_one_upper_v_variable_no_exist() {
  ARTTree<int> root = ARTTree<int>();
  int val = 0;
  bool success = root.lookup(KEY2, val);
  assert(!success && val == 0 && "test failed");
}

void test_length_more_than_one_low_nullterm_v_variable_exists() {
  ARTTree<int> root = ARTTree<int>();
  root.insert(KEY8, VAL8);
  int val = 0;
  bool success = root.lookup(KEY8, val);
  assert(success && val == VAL8 && "test failed");
}

void test_length_more_than_one_upper_nullterm_v_variable_exists() {
  ARTTree<int> root = ARTTree<int>();
  root.insert(KEY9, VAL4);
  int val = 0;
  bool success = root.lookup(KEY9, val);
  assert(success && val == VAL4 && "test failed");
}

void test_more_than_four_keys_going_through_same_node() {
  ARTTree<int> root = ARTTree<int>();
  root.insert(KEY10, VAL);
  root.insert(KEY11, VAL2);
  root.insert(KEY12, VAL3);
  root.insert(KEY13, VAL4);
  root.insert(KEY14, VAL5);
  
  int val1 = 0;
  int val2 = 0;
  int val3 = 0;
  int val4 = 0;
  int val5 = 0;
  
  bool success1 = root.lookup(KEY10, val1);
  bool success2 = root.lookup(KEY11, val2);
  bool success3 = root.lookup(KEY12, val3);
  bool success4 = root.lookup(KEY13, val4);
  bool success5 = root.lookup(KEY14, val5);

  assert(success1 && val1 == VAL && "test failed");
  assert(success2 && val2 == VAL2 && "test failed");
  assert(success3 && val3 == VAL3 && "test failed");
  assert(success4 && val4 == VAL4 && "test failed");
  assert(success5 && val5 == VAL5 && "test failed");
}

// Tests for update

void test_length_zero_low_value_no_exist2() {
  ARTTree<int> root = ARTTree<int>();
  root.update(KEY, VAL);   // Assumes update will create the node
  int val = 0;
  bool success = root.lookup(KEY, val);
  assert(success && val == VAL && "test failed");
}

void test_length_one_low_value_exists2() {
  ARTTree<int> root = ARTTree<int>();
  root.insert(KEY2, VAL2);
  root.update(KEY2, VAL3);
  int val = 0;
  bool success = root.lookup(KEY2, val);
  assert(success && val == VAL3 && "test failed");
}

void test_length_more_than_one_med_value_exists() {
  ARTTree<int> root = ARTTree<int>();
  root.insert(KEY3, VAL3);
  root.update(KEY3, VAL4);
  int val = 0;
  bool success = root.lookup(KEY3, val);
  assert(success && val == VAL4 && "test failed");
}

void test_length_more_than_one_med_value_no_exist3() {
  ARTTree<int> root = ARTTree<int>();
  root.update(KEY4, VAL7);
  int val = 0;
  bool success = root.lookup(KEY4, val);
  assert(success && val == VAL7 && "test failed"); // Assumes update will create the node.
}

void test_length_more_than_one_large_value_exists3() {
  ARTTree<int> root = ARTTree<int>();
  root.insert(KEY5, VAL6);
  root.update(KEY5, VAL6);
  int val = 0;
  bool success = root.lookup(KEY5, val);
  assert(success && val == VAL6 && "test failed");
}

void test_length_more_than_one_large_value_no_exist() {
  ARTTree<int> root = ARTTree<int>();
  root.update(KEY6, VAL8);
  int val = 0;
  bool success = root.lookup(KEY6, val);
  assert(success && val == VAL8 && "test failed");

}

// Tests for remove

void test_length_zero_no_exist() {
  ARTTree<int> root = ARTTree<int>();
  root.remove(KEY);
  int val = 0;
  bool success = root.lookup(KEY, val);
  assert(!success && val == 0 && "test failed");
}

void test_length_one_exists() {
  ARTTree<int> root = ARTTree<int>();
  root.insert(KEY2, VAL3);
  root.remove(KEY2);
  int val = 0;
  bool success = root.lookup(KEY2, val);
  assert(!success && val == 0 && "test failed");
}

void test_length_more_than_one_exists() {
  ARTTree<int> root = ARTTree<int>();
  root.insert(KEY3, VAL7);
  root.remove(KEY3);
  int val = 0;
  bool success = root.lookup(KEY3, val);
  assert(!success && val == 0 && "test failed");
}

void test_length_more_than_one_no_exist() {
  ARTTree<int> root = ARTTree<int>();
  root.remove(KEY4);
  int val = 0;
  bool success = root.lookup(KEY4, val);
  assert(!success && val == 0 && "test failed");
}

void* insert(void* pair) {
  ((insert_info *)pair)->tree->insert(((insert_info *)pair)->key, ((insert_info *)pair)->val);

  return NULL;
}

void* update(void* pair) {
  ((insert_info *)pair)->tree->update(((insert_info *)pair)->key, ((insert_info *)pair)->val);

  return NULL;
}

void* lookup(void* params) {
  ((lookup_info *)params)->success = ((lookup_info *)params)->tree->lookup(((lookup_info *)params)->key, ((lookup_info *)params)->retval);
  return params;
}

// Accepts insert_info pointer in the form of void*. Asserts lookup succeeded.
void* insert_then_lookup(void* params) {
  insert(params);
  int retval = 0;
  bool success = ((insert_info *)params)->tree->lookup(((insert_info *)params)->key, retval);
  assert(success && retval == ((insert_info *)params)->val && "Lookup failed or did not return value expected");

  return NULL;
}

std::string generate_key() {
  std::string key = "";
  for(int i = 0; i < 800; i++) {
    double fraction = std::rand()/(double)RAND_MAX;
    // Generate chars with int value 1 to 127. Why? Because 127 is the highest int value for regular char type, and we do not want to generate 0 because that is the value of the null terminator.
    double random_char_value = std::round(fraction * 126) + 1;
    key.push_back((char)((int)random_char_value));
  }
  return key;
}

int generate_val() {
  return std::rand();
}

void test_insert_then_lookup_random_keys() {
  const int num_threads = 100;
  pthread_t* threads[num_threads];
  insert_info* info[num_threads];
  ARTTree<int>* tree = new ARTTree<int>();

  // Create threads to insert random values into the tree.
  for (int i = 0; i < num_threads; i++) {
    info[i] = new insert_info({generate_key(), generate_val(), tree});
    threads[i] = new pthread_t();
    pthread_create(threads[i], NULL, &insert, (void *)info[i]);
  }

  // Wait for all threads to finish
  void* result;
  for (int i = 0; i < num_threads; i++) {
    pthread_join(*threads[i],&result);
  }

  // Check that all of the inserts actually inserted the keys correctly.
  // Because keys are randomly generated, there is the possibility that a randomly
  // generated key is the same as another randomly generated key.
  // However, this probability is unlikely for randomly generated
  // keys over 8 characters long.
  for (int i = 0; i < num_threads; i++) {
    int retval;
    assert(info[i]->tree->lookup(info[i]->key, retval) &&
           retval == info[i]->val &&
           "Lookup not successful, or retval not equal to actual val");
  }
}

void test_insert_then_lookup_unique_keys() {
  
  const int num_threads = 128;
  pthread_t* threads[num_threads];
  insert_info* info[num_threads];
  ARTTree<int>* tree = new ARTTree<int>();

  // Create threads to insert unique keys with values into the tree.
  for (int i = 0; i < num_threads; i++) {
    std::string str = "";
    str.push_back((char)i);
    info[i] = new insert_info({str, generate_val(), tree});
    threads[i] = new pthread_t();
    pthread_create(threads[i], NULL, &insert, (void *)info[i]);
  }

  // Wait for all threads to finish
  void* result;
  for (int i = 0; i < num_threads; i++) {
    pthread_join(*threads[i],&result);
  }

  // Check that all of the inserts actually inserted the keys correctly.
  for (int i = 0; i < num_threads; i++) {
    int retval;
    assert(info[i]->tree->lookup(info[i]->key, retval) &&
           retval == info[i]->val &&
           "Lookup not successful, or retval not equal to actual val");
  }
}

void test_insert_then_lookup_unique_keys_and_node_expansion() {
  const int num_threads = 128;
  pthread_t* threads[num_threads];
  insert_info* info[num_threads];
  ARTTree<int>* tree = new ARTTree<int>();

  // Create threads to insert unique keys with values into the tree.
  for (int i = 0; i < num_threads; i++) {
    // All keys will start with same prefix, which will grow the internal nodes
    // upon multiple inserts.
    std::string str = "abcde";
    str.push_back((char)i);
    info[i] = new insert_info({str, generate_val(), tree});
    threads[i] = new pthread_t();
    pthread_create(threads[i], NULL, &insert, (void *)info[i]);
  }

  // Wait for all threads to finish
  void* result;
  for (int i = 0; i < num_threads; i++) {
    pthread_join(*threads[i],&result);
  }

  // Check that all of the inserts actually inserted the keys correctly.
  for (int i = 0; i < num_threads; i++) {
    int retval;
    assert(info[i]->tree->lookup(info[i]->key, retval) &&
           retval == info[i]->val &&
           "Lookup not successful, or retval not equal to actual val");
  }
}

void test_insert_then_update_unique_keys() {
  const int num_threads = 128;
  pthread_t* threads[num_threads];
  insert_info* info[num_threads];
  ARTTree<int>* tree = new ARTTree<int>();

  // Create threads to insert unique keys with values into the tree.
  for (int i = 0; i < num_threads; i++) {
    std::string str = "";
    str.push_back((char)i);
    info[i] = new insert_info({str, generate_val(), tree});
    threads[i] = new pthread_t();
    pthread_create(threads[i], NULL, &insert, (void *)info[i]);
  }

  // Wait for all threads to finish
  void* result;
  for (int i = 0; i < num_threads; i++) {
    pthread_join(*threads[i],&result);
  }

  // Create threads to update the unique keys with values into the tree.
  for (int i = 0; i < num_threads; i++) {
    std::string str = "";
    str.push_back((char)i);
    info[i]->val /= 2;
    pthread_create(threads[i], NULL, &update, (void *)info[i]);
  }

  // Wait for all threads to finish
  for (int i = 0; i < num_threads; i++) {
    pthread_join(*threads[i],&result);
  }


  // Check that all of the inserts actually inserted the keys correctly.
  for (int i = 0; i < num_threads; i++) {
    int retval;
    assert(info[i]->tree->lookup(info[i]->key, retval) &&
           retval == info[i]->val &&
           "Lookup not successful, or retval not equal to actual val");
  }
}

void test_lookup_while_insert_unique_keys_common_prefix() {
  const int num_threads = 128;
  pthread_t* threads[num_threads];
  insert_info* info[num_threads];
  ARTTree<int>* tree = new ARTTree<int>();

  // Create threads to insert unique keys with values into the tree.
  // Strings will contain common prefix to force more collision to
  // test that lookup works as intended.
  for (int i = 0; i < num_threads; i++) {
    std::string str = "abcdefghijklmnopqrstuvwxyz";
    str.push_back((char)i);
    info[i] = new insert_info({str, generate_val(), tree});
    threads[i] = new pthread_t();
    pthread_create(threads[i], NULL, &insert_then_lookup, (void *)info[i]);
  }

  // Wait for all threads to finish
  void* result;
  for (int i = 0; i < num_threads; i++) {
    pthread_join(*threads[i],&result);
  }

  // Check that all of the inserts actually inserted the keys correctly.
  for (int i = 0; i < num_threads; i++) {
    int retval;
    assert(info[i]->tree->lookup(info[i]->key, retval) &&
           retval == info[i]->val &&
           "Lookup not successful, or retval not equal to actual val");
  }
}

int main() {

  // Tests for lookup
 test_length_zero_low_value_no_exist();
 test_length_one_low_value_exists();
 test_length_more_than_one_med_value_no_exist();
 test_length_more_than_one_large_value_exists();
 test_length_more_than_one_med_value_no_exist2();
 test_length_more_than_one_large_value_exists2();
 
 // Tests for insert
 test_length_zero_v_variable_exists();
 test_length_one_upper_v_variable_exists();
 test_length_more_than_one_low_upper_punc_v_variable_exists();
 //test_length_more_than_one_low_sym_upper_punc_null_exists();
 //test_length_one_upper_literal_exists();
 //test_length_more_than_one_low_non_v_literal_exists();
 test_length_more_than_one_punc_lower_sym_v_variable_no_exist();
 test_length_one_upper_v_variable_no_exist();
 test_length_more_than_one_low_nullterm_v_variable_exists();
 test_length_more_than_one_upper_nullterm_v_variable_exists();
 test_more_than_four_keys_going_through_same_node();

 // Tests for update
 test_length_zero_low_value_no_exist2();
 test_length_one_low_value_exists2();
 test_length_more_than_one_med_value_exists();
 test_length_more_than_one_med_value_no_exist3();
 test_length_more_than_one_large_value_exists3();
 test_length_more_than_one_large_value_no_exist();
 
 // Tests for remove
 // Note: The ART does not actually remove the elements
 // anymore - it just sets a flag in the TransItem that
 // will allow for actually clearing of the record entry
 // in the after commit phase.
 
 // test_length_zero_no_exist();
 // test_length_one_exists();
 // test_length_more_than_one_exists();
 // test_length_more_than_one_no_exist();

 // Concurrency tests
 test_insert_then_lookup_unique_keys();
 test_insert_then_lookup_random_keys();
 test_insert_then_lookup_unique_keys_and_node_expansion();
 test_insert_then_update_unique_keys();
 test_lookup_while_insert_unique_keys_common_prefix();
 
 return 0;
}
