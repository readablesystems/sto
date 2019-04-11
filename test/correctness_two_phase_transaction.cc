// Turns on extra correctness checks when running these tests.
#define PHASE_TWO_VALIDATE 1

#include <csignal>
#include <iostream>
#include <stdlib.h>
#include <string>
#include <unordered_set>
#include "Transaction.hh"
#include "DB_index.hh"
#include "DB_params.hh"

// Row declaration
struct row;

#define UNORDERED_INDEX 1
#define ORDERED_INDEX 2

// Change between macros to test different methods
#define INDEX ORDERED_INDEX

#if INDEX == UNORDERED_INDEX
typedef bench::unordered_index<std::string, int, db_params::db_default_params> index_type;
typedef int value_type;
#define SELECT(KEY) database.select_row(KEY)
#define SELECT_FOR_UPDATE(KEY) database.select_row(KEY, true)
#define SELECT_SUBACTION(KEY) database.select_row_subaction(KEY)
#define ASSERT_VALUES_EQUAL(LVALUE, RVALUE) assert(*(LVALUE) == *(RVALUE));

#elif INDEX == ORDERED_INDEX
typedef bench::ordered_index<std::string, row, db_params::db_default_params> index_type;
typedef row value_type;
#define SELECT(KEY) database.select_row(KEY, bench::RowAccess::ObserveValue)
#define SELECT_FOR_UPDATE(KEY) database.select_row(KEY, bench::RowAccess::UpdateValue)
#define SELECT_SUBACTION(KEY) database.select_row_subaction(KEY, bench::RowAccess::ObserveValue)
#define ASSERT_VALUES_EQUAL(LVALUE, RVALUE) assert((LVALUE)->value == (RVALUE)->value);
#endif

#define INITIALIZE_INDEX index_type database = index_type(2048);
#define INSERT(KEY, VALUE) database.insert_row(KEY, VALUE)
#define UPDATE(ROW_PTR, VALUE) database.update_row((ROW_PTR), (VALUE))
#define DELETE(KEY) database.delete_row(KEY)
#define PRINT_PASS std::cout << "PASS: " << __FUNCTION__ << std::endl;


// Struct to easily fill get the return values from select_row_subaction
struct sel_return_subaction_struct {
  bool success;
  bool found;
  uintptr_t row_ptr;
  const value_type* value;
  int subaction_id;

  sel_return_subaction_struct
  (std::tuple<bool, bool, uintptr_t, const value_type*, int> select_return_subaction) {
    std::tie(success, found, row_ptr, value, subaction_id) = 
      select_return_subaction;
  }

  // Returns whether the the method was successful, record was found, and the
  // subaction_id is non-negative.
  bool good() {
    return success && found && (subaction_id >= 0);
  }
};

// Struct to easily fill get the return values from select_row
struct sel_return_struct {
  bool success;
  bool found;
  uintptr_t row_ptr;
  const value_type* value;

  sel_return_struct
  (std::tuple<bool, bool, uintptr_t, const value_type*> select_return) {
    std::tie(success, found, row_ptr, value) = select_return;
  }

  bool good() {
    return success && found;
  }
};

// Row definition - used for in 'ordered_index' methods
struct row {
  enum class NamedColumn : int {name = 0};
  int value;

  row(): value() {}
  row(int val): value(val) {}
};

// Some sample keys and values
const std::string key1 = "teststring";
const std::string key2 = "teststring2";
const std::string key3 = "teststring3";
const std::string key4 = "teststring4";
const std::string key5 = "teststring5";
const std::string key6 = "teststring6";
const std::string key7 = "teststring7";
const std::string key8 = "teststring8";

value_type* const value1 = new value_type(100);
value_type* const value2 = new value_type(50);
value_type* const value3 = new value_type(25);
value_type* const value4 = new value_type(10);
value_type* const value5 = new value_type(5);
value_type* const value6 = new value_type(6);
value_type* const value7 = new value_type(7);
value_type* const value8 = new value_type(8);

// Test that just starting a two phase transaction works
void testStartEmptyTwoPhaseTransaction() {
  {
    TransactionGuard t;
    Sto::start_two_phase_transaction();
    // Nothing happens here
  }
  PRINT_PASS
}

// Test that the skeleton of the two phase transaction works
void testStartEmpty2PTWithPhaseTwo() {
  {
    TransactionGuard t;
    Sto::start_two_phase_transaction();
    // Nothing happens here
    Sto::phase_two();
    // Nothing happens here
  }
  PRINT_PASS
}

// Test that running one subaction to find a row succeeds, finds the record, and
// creates a subaction.
void testSubactionWithOneElement() {

  INITIALIZE_INDEX
  // Initial transaction inserts the element.
  {
    TransactionGuard t;
    INSERT(key1, value1);
  }

  // Two phase transaction successfully reads the element, creating a subaction
  // for reading the record.
  {
    TransactionGuard t;
    Sto::start_two_phase_transaction();

    sel_return_subaction_struct ret(SELECT_SUBACTION(key1));
    assert(ret.success);
    assert(ret.found);
    assert(ret.value != NULL);
    ASSERT_VALUES_EQUAL(ret.value, value1)
    assert(ret.subaction_id == 0);

    Sto::phase_two();
    // Nothing happens here
  }
  PRINT_PASS
}

// Test that the a subaction with multiple elements works
void testMultipleSubactionsWithOneElement() {

  INITIALIZE_INDEX
  // Initial transaction inserts the element.
  {
    TransactionGuard t;
    INSERT(key1, value1);
    INSERT(key2, value2);
    INSERT(key3, value3);
    INSERT(key4, value4);
  }

  // Two phase transaction successfully reads all elements, creating a subaction
  // for each record.
  {
    TransactionGuard t;
    Sto::start_two_phase_transaction();

    sel_return_subaction_struct ret(SELECT_SUBACTION(key1));
    assert(ret.success);
    assert(ret.found);
    assert(ret.value != NULL);
    ASSERT_VALUES_EQUAL(ret.value, value1)
    assert(ret.subaction_id >= 0);

    sel_return_subaction_struct ret2(SELECT_SUBACTION(key2));
    assert(ret2.success);
    assert(ret2.found);
    assert(ret2.value != NULL);
    ASSERT_VALUES_EQUAL(ret2.value, value2)
    assert(ret2.subaction_id != ret.subaction_id);
    assert(ret2.subaction_id >= 0);

    sel_return_subaction_struct ret3(SELECT_SUBACTION(key3));
    assert(ret3.success);
    assert(ret3.found);
    assert(ret3.value != NULL);
    ASSERT_VALUES_EQUAL(ret3.value, value3)
    assert(ret3.subaction_id != ret2.subaction_id);
    assert(ret3.subaction_id != ret.subaction_id);
    assert(ret3.subaction_id >= 0);

    sel_return_subaction_struct ret4(SELECT_SUBACTION(key4));
    assert(ret4.success);
    assert(ret4.found);
    assert(ret4.value != NULL);
    ASSERT_VALUES_EQUAL(ret4.value, value4)
    assert(ret4.subaction_id != ret3.subaction_id);
    assert(ret4.subaction_id != ret2.subaction_id);
    assert(ret4.subaction_id != ret.subaction_id);
    assert(ret4.subaction_id >= 0);

    Sto::phase_two();
    // Nothing happens here
  }
  PRINT_PASS
}

void testMultipleSubactionsWithMultipleElements() {
  INITIALIZE_INDEX

  // Create another database that will be looked through in each new subaction.
  bench::unordered_index<int, int, db_params::db_default_params> 
    database2(2048);

  // Define the return type of 'database2'
  struct database2_return_struct {
    bool success;
    bool found;
    uintptr_t row_ptr;
    const int* value;

    database2_return_struct
    (std::tuple<bool, bool, uintptr_t, const int*> select_return) {
      std::tie(success, found, row_ptr, value) = select_return;
    }
  };
          
  {
    TransactionGuard t;
    INSERT(key1, value1);
    INSERT(key2, value2);
    INSERT(key3, value3);
    INSERT(key4, value4);

#if INDEX == UNORDERED_INDEX    
    database2.insert_row(*value1, value5);
    database2.insert_row(*value2, value6);
    database2.insert_row(*value3, value7);
    database2.insert_row(*value4, value8);
#elif INDEX == ORDERED_INDEX
    database2.insert_row(value1->value, &value5->value);
    database2.insert_row(value2->value, &value6->value);
    database2.insert_row(value3->value, &value7->value);
    database2.insert_row(value4->value, &value8->value);
#endif
  }

  // Create a list to loop through all the keys
  int size = 4;
  const std::string keys[size] = { key1, key2, key3, key4 };

  // Create a set to store all keys of 'database' that lead to even values from
  // 'database2'.
  std::unordered_set<std::string> even_value_keys;

  // Add all even values to a set and delete all keys in 'database1' that led to
  // odd values in 'database2'.
  {
    TransactionGuard t;
    Sto::start_two_phase_transaction();

    
    for(int i = 0; i < size; i++) {
      sel_return_subaction_struct sret(SELECT_SUBACTION(keys[i]));
      assert(sret.good());
#if INDEX == UNORDERED_INDEX      
      database2_return_struct ret(database2.select_row(*sret.value));
#elif INDEX == ORDERED_INDEX            
      database2_return_struct ret(database2.select_row(sret.value->value));
#endif
      assert(ret.success);
      assert(ret.found);
      if (*ret.value % 2 == 0) {
        even_value_keys.insert(keys[i]);
      } else {
        Sto::abort_subaction(sret.subaction_id);
      }
    }

    Sto::phase_two();
    for (auto key : even_value_keys) {
      DELETE(key);
    }
  }
  PRINT_PASS
}

void testNonsubactionReadFollowedBySubactionRead() {
  INITIALIZE_INDEX
  // Initial transaction inserts the element.
  {
    TransactionGuard t;
    INSERT(key1, value1);
    INSERT(key2, value2);
  }

  {
    TransactionGuard t;
    Sto::start_two_phase_transaction();

    sel_return_struct ret(SELECT(key1));
    assert(ret.success);
    assert(ret.found);
    ASSERT_VALUES_EQUAL(ret.value, value1)

    sel_return_subaction_struct sret(SELECT_SUBACTION(key2));
    assert(sret.success);
    assert(sret.found);
    ASSERT_VALUES_EQUAL(sret.value, value2)
    assert(sret.subaction_id >= 0);

    Sto::phase_two();
    // Nothing happens here
  }
  PRINT_PASS
}

void testMultipleNonsubactionReadsWithMultiplesSubactionReads() {

  INITIALIZE_INDEX
  // Initial transaction inserts the element.
  {
    TransactionGuard t;
    INSERT(key1, value1);
    INSERT(key2, value2);
    INSERT(key3, value3);
    INSERT(key4, value4);
  }

  {
    TransactionGuard t;
    Sto::start_two_phase_transaction();
    sel_return_subaction_struct sret(SELECT_SUBACTION(key1));
    assert(sret.success);
    assert(sret.found);
    ASSERT_VALUES_EQUAL(sret.value, value1)
    assert(sret.subaction_id >= 0);
    
    sel_return_struct ret(SELECT(key2));
    assert(ret.success);
    assert(ret.found);
    ASSERT_VALUES_EQUAL(ret.value, value2)

    sel_return_subaction_struct sret2(SELECT_SUBACTION(key3));
    assert(sret2.success);
    assert(sret2.found);
    ASSERT_VALUES_EQUAL(sret2.value, value3)
    assert(sret2.subaction_id >= 0);
    assert(sret2.subaction_id != sret.subaction_id);

    sel_return_struct ret2(SELECT(key4));
    assert(ret2.success);
    assert(ret2.found);
    ASSERT_VALUES_EQUAL(ret2.value, value4)

    Sto::phase_two();
    // Nothing happens here
  }
  PRINT_PASS
}

void testTwoTransactionReadSameSubactionItemsCommitDifferently() {

  INITIALIZE_INDEX
  // Initial transaction inserts the element.
  {
    TransactionGuard t;
    INSERT(key1, value1);
    INSERT(key2, value2);
    INSERT(key3, value3);
    INSERT(key4, value4);
  }

  // Have subaction accesses fluctuate between transactions

  {  
    TestTransaction t1(1);
    Sto::start_two_phase_transaction();
    sel_return_subaction_struct sret11(SELECT_SUBACTION(key1));
    assert(sret11.good());
    sel_return_subaction_struct sret12(SELECT_SUBACTION(key2));
    assert(sret12.good());

    TestTransaction t2(2);
    Sto::start_two_phase_transaction();
    sel_return_subaction_struct sret21(SELECT_SUBACTION(key1));
    assert(sret21.good());
    sel_return_subaction_struct sret22(SELECT_SUBACTION(key2));
    assert(sret22.good());

    t1.use();
    sel_return_subaction_struct sret13(SELECT_SUBACTION(key3));
    assert(sret13.good());
    sel_return_subaction_struct sret14(SELECT_SUBACTION(key4));
    assert(sret14.good());
    
    t2.use();
    sel_return_subaction_struct sret23(SELECT_SUBACTION(key3));
    assert(sret23.good());
    sel_return_subaction_struct sret24(SELECT_SUBACTION(key4));
    assert(sret24.good());

    // t1 is only interested in 'key1' and 'key2'
    t1.use();
    Sto::abort_subaction(sret13.subaction_id);
    Sto::abort_subaction(sret14.subaction_id);

    // t2 is only interested in 'key3' and 'key4'
    t2.use();
    Sto::abort_subaction(sret21.subaction_id);
    Sto::abort_subaction(sret22.subaction_id);

    t1.use();
    Sto::phase_two(); // Called for correctness checks
    assert(t1.try_commit());

    t2.use();
    Sto::phase_two(); // Called for correctness checks
    assert(t2.try_commit());
  }
  PRINT_PASS
}

void testSubactionAbort() {

  INITIALIZE_INDEX
  // Initial transaction inserts the element.
  {
    TransactionGuard t;
    INSERT(key1, value1);
  }

  {
    TransactionGuard t;
    Sto::start_two_phase_transaction();
    
    sel_return_subaction_struct ret(SELECT_SUBACTION(key1));
    assert(ret.good());
    ASSERT_VALUES_EQUAL(ret.value, value1)
    Sto::abort_subaction(ret.subaction_id);

    Sto::phase_two();
    // Nothing happens here
  }
  PRINT_PASS
}

void testSubactionsAbort() {

  INITIALIZE_INDEX  
  // Initial transaction inserts the element.
  {
    TransactionGuard t;
    INSERT(key1, value1);
    INSERT(key2, value2);
    INSERT(key3, value3);
    INSERT(key4, value4);
  }

  {
    TransactionGuard t;
    Sto::start_two_phase_transaction();
    
    sel_return_subaction_struct ret(SELECT_SUBACTION(key1));
    assert(ret.good());
    ASSERT_VALUES_EQUAL(ret.value, value1)
    Sto::abort_subaction(ret.subaction_id);

    sel_return_subaction_struct ret2(SELECT_SUBACTION(key2));
    assert(ret2.good());
    ASSERT_VALUES_EQUAL(ret2.value, value2)
    Sto::abort_subaction(ret2.subaction_id);

    // We won't abort this subaction
    sel_return_subaction_struct ret3(SELECT_SUBACTION(key3));
    assert(ret3.good());
    ASSERT_VALUES_EQUAL(ret3.value, value3)

    sel_return_subaction_struct ret4(SELECT_SUBACTION(key4));
    assert(ret4.good());
    ASSERT_VALUES_EQUAL(ret4.value, value4)
    Sto::abort_subaction(ret4.subaction_id);

    Sto::phase_two();
    // Nothing happens here
  }
  PRINT_PASS
}

// If the optimization in 'abort_subaction' (which overwrites the TransItems of
// an aborted subaction) is on (which it is), this method will pass because it
// does not remember the invalidated item at all. Without the optimization, Sto
// will see that the invalidated item is being accessed again, and it will abort
// through an assertion failure.
void testUpdateOnInvalidItem() {
  INITIALIZE_INDEX
  {
    TransactionGuard t; 
    INSERT(key1, value1);
    INSERT(key2, value2);
  }

  {
    TransactionGuard t;
    Sto::start_two_phase_transaction();

    sel_return_subaction_struct ret(SELECT_SUBACTION(key1));
    assert(ret.good());
    ASSERT_VALUES_EQUAL(ret.value, value1)

    // This is the subaction item we will try to access
    sel_return_struct ret2(SELECT(key2));
    assert(ret2.good());
    ASSERT_VALUES_EQUAL(ret2.value, value2)

    // Abort subaction
    Sto::abort_subaction(ret.subaction_id);

    Sto::phase_two();

    // Modify the invalid record.
    DELETE(key2);
  }
  PRINT_PASS
}

// Define database parameters to use in test 'testOrderedIndexRangeScan'. I
// didn't use 'db_default_params' because it didn't enable read my writes, which
// I rely on in this test.
class db_test_params {
public:
  static constexpr db_params::db_params_id Id = db_params::db_params_id::Default;
  static constexpr bool RdMyWr = true;
  static constexpr bool TwoPhaseLock = false;
  static constexpr bool Adaptive = false;
  static constexpr bool Opaque = false;
  static constexpr bool Swiss = false;
  static constexpr bool TicToc = false;
};

void testOrderedIndexRangeScan() {
  // Test specifically the 'ordered_index' range scan because 'unordered_index'
  // doesn't define it.
  typedef bench::ordered_index<std::string, row, db_test_params> 
    ordered_index_type;
  ordered_index_type ordered_database = ordered_index_type(2048);

  // Define all ordered keys and values used in this test. Note we define them
  // here because we're not going to use them in any other test.
  std::string okey1 = "b";
  std::string okey2 = "c";
  std::string okey3 = "d";
  std::string okey4 = "e";

  row* ovalue1 = new row(1);
  row* ovalue2 = new row(2);
  row* ovalue3 = new row(3);
  row* ovalue4 = new row(4);

  // Define the lower and upper bounds of the keys to input for
  // 'range_scan_subaction'.
  const std::string upper_bound = "a";
  const std::string lower_bound = "f";
  
  {
    TransactionGuard t;
    ordered_database.insert_row(okey1, ovalue1);
    ordered_database.insert_row(okey2, ovalue2);
    ordered_database.insert_row(okey3, ovalue3);
    ordered_database.insert_row(okey4, ovalue4);
  }

  {
    TransactionGuard t;
    Sto::start_two_phase_transaction();
    
    // Call 'range_scan_subaction'. To do this, we need to create a callback
    // function and put all our functionality there.

    // Initialize set storing keys with even values.
    std::unordered_set<std::string> even_value_keys;

    // Create a closure that will allow us to reference and modify
    // 'even_value_keys'.
    auto scan_callback =
      [&] (const std::string& key, const row& val, const int subaction_id) {
        assert(subaction_id >= 0);
        if(val.value % 2 == 0) {
          even_value_keys.insert(key);
        } else {
          Sto::abort_subaction(subaction_id);
        }
        return true;
      };

    bool success = ordered_database.
      template range_scan_subaction
      <decltype(scan_callback), false>
      (upper_bound, lower_bound, scan_callback, bench::RowAccess::ObserveExists);
    
    assert(success);
    Sto::phase_two();

    // Delete the keys with even values.
    for (auto key : even_value_keys) {
      ordered_database.delete_row(key);
    }

    auto check_assertions_callback =
      [&] (const std::string& key, const row& val) {
        (void)key;
        assert(val.value % 2 != 0);
        return true;
      };

    // Check that all the values in the database only have odd values.
    bool success2 = ordered_database.template range_scan
      <decltype(check_assertions_callback), false>
      (upper_bound, lower_bound, check_assertions_callback,
       bench::RowAccess::ObserveExists);

    assert(success2);
  }
  PRINT_PASS
}

void testSecondPhaseUpdateValue() {
  INITIALIZE_INDEX

  {
    TransactionGuard t;
    INSERT(key1, value1);
    INSERT(key2, value2);
  }

  {
    TransactionGuard t;
    Sto::start_two_phase_transaction();
    
    // Nothing is done here.
    
    Sto::phase_two();

    sel_return_struct ret(SELECT_FOR_UPDATE(key1));
    sel_return_struct ret2(SELECT_FOR_UPDATE(key2));

    assert(ret.good());
    assert(ret2.good());
    ASSERT_VALUES_EQUAL(ret.value, value1);
    ASSERT_VALUES_EQUAL(ret2.value, value2);

    UPDATE(ret.row_ptr, value3);
    UPDATE(ret2.row_ptr, value4);
  }

  {
    TransactionGuard t;
    sel_return_struct ret(SELECT(key1));
    sel_return_struct ret2(SELECT(key2));

    assert(ret.good());
    assert(ret2.good());
    ASSERT_VALUES_EQUAL(ret.value, value3);
    ASSERT_VALUES_EQUAL(ret2.value, value4);
  }

  PRINT_PASS
}

void testSecondPhaseEmptyLookup() {
  INITIALIZE_INDEX

  // No keys inserted into the index.
  {
    TransactionGuard t;
    Sto::start_two_phase_transaction();

    // Nothing happens here

    Sto::phase_two();

    sel_return_struct ret(SELECT(key1));
    assert(ret.success);
    assert(!ret.found);

    sel_return_struct ret2(SELECT(key2));
    assert(ret2.success);
    assert(!ret2.found);
  }

  PRINT_PASS
}

void testSecondPhaseSubactionMethodCallFails() {
  INITIALIZE_INDEX  
  {
    TransactionGuard t;
    Sto::start_two_phase_transaction();

    // Nothing happens here.

    Sto::phase_two();

    // Expect an error coming from this. Doesn't create an error because it
    // fails and doesn't actually create a new subaction. But should this still
    // be allowed?
    sel_return_subaction_struct sret(SELECT_SUBACTION(key1));
  }
  PRINT_PASS
}

void testTsetSizeConsistency() {
  INITIALIZE_INDEX
  {
    TransactionGuard t;
    INSERT(key1, value1);
    INSERT(key2, value2);
  }

  {
    TransactionGuard t;
    Sto::start_two_phase_transaction();

    SELECT_SUBACTION(key1);
    SELECT(key2);
    
    Sto::phase_two();

    assert(Sto::get_tset_size() == 2);
  }    
  PRINT_PASS
}

void testTsetSizeConsistencyRangeScan() {
  // Define all ordered keys used in this test. Note we define them
  // here because we're not going to use them in any other test.
  std::string okey1 = "b";
  std::string okey2 = "c";
  std::string okey3 = "d";
  std::string okey4 = "e";
  std::string okey5 = "f";
  std::string okey6 = "g";
  std::string okey7 = "h";
  std::string okey8 = "i";

  // Define the lower and upper bounds of the keys to input for
  // 'range_scan_subaction'.
  const std::string upper_bound = "a";
  const std::string lower_bound = "j";

  INITIALIZE_INDEX
  {
    TransactionGuard t;
    INSERT(okey1, value1);
    INSERT(okey2, value2);
    INSERT(okey3, value3);
    INSERT(okey4, value4);
    INSERT(okey5, value5);
    INSERT(okey6, value6);
    INSERT(okey7, value7);
    INSERT(okey8, value8);
  }

  {
    TransactionGuard t;
    Sto::start_two_phase_transaction();

    auto simple_callback =
      [&] (const std::string& key, const row& val, int subaction_id) {
        (void)key;
        (void)val;
        (void)subaction_id;
        return true;
      };

    bool success = database.template range_scan_subaction
      <decltype(simple_callback), false>
      (upper_bound, lower_bound, simple_callback,
       bench::RowAccess::ObserveExists);
    assert(success);

    Sto::phase_two();

    assert(Sto::get_tset_size() == 8);
  }
  PRINT_PASS
}

int main() {
  testStartEmptyTwoPhaseTransaction();
  testStartEmpty2PTWithPhaseTwo();
  testSubactionWithOneElement();
  testMultipleSubactionsWithOneElement();
  testMultipleSubactionsWithMultipleElements();
  testNonsubactionReadFollowedBySubactionRead();
  testMultipleNonsubactionReadsWithMultiplesSubactionReads();
  testTwoTransactionReadSameSubactionItemsCommitDifferently();
  testSubactionAbort();
  testSubactionsAbort();
  testUpdateOnInvalidItem();
  // testOrderedIndexRangeScan();
  testSecondPhaseUpdateValue();
  testSecondPhaseEmptyLookup();

  testSecondPhaseSubactionMethodCallFails();

  testTsetSizeConsistency();
  // testTsetSizeConsistencyRangeScan();
  
  std::cout << "All tests passed!" << std::endl;
  return 0;
}
