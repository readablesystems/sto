#undef NDEBUG
#include <cassert>
#include <string>
#include <iostream>
#include "StringWrapper.hh"
#include "Transaction.hh"
#include "ARTTree.hh"

#define GUARDED if (TransactionGuard tguard{})

void testSimpleString() {
  ARTTree<int> tree;
  std::string s1 = "simple";
  int v1 = 5;
  int retval;

  {
    TransactionGuard* t1 = new TransactionGuard();
    tree.insert(s1, v1);
    delete t1;
  }

  {
    TransactionGuard* t2 = new TransactionGuard();
    bool success = tree.lookup(s1, retval);
    assert(success && retval == v1);
    delete t2;
  }

  std::cout << "PASS: " <<  __FUNCTION__ << std::endl;
}

void testConcurrentInsertLookup() {
  ARTTree<int> tree;
  std::string s1 = "string1";
  int v1 = 92;
  
  std::string s2 = "string2";
  int v2 = 41;

  int v3 = 10;

  int retval;

  // Blind writes - initially inserts values
  {
    TestTransaction t1(1);
    tree.insert(s1, v1);

    TestTransaction t2(2);
    tree.insert(s2, v2);

    assert(t2.try_commit());
    assert(t1.try_commit());
  }

  {
    TestTransaction t1(1);
    tree.lookup(s1, retval);

    TestTransaction t2(2);
    tree.update(s1, v3);
    tree.lookup(s1, retval);
    assert(t2.try_commit());
    assert(!t1.try_commit()); 
  }

  std::cout << "PASS: " << __FUNCTION__ << std::endl;
}

void testPopulateTreeAndLookup() {
  ARTTree<int> tree;
  std::string s1 = "string1";
  std::string s2 = "string2";
  std::string s3 = "string3";
  std::string s4 = "string4";
  std::string s5 = "string5";
  std::string s6 = "string6";
  
  int v1 = 92;
  int v2 = 41;
  int v3 = 10;
  int v4 = 1000;
  int v5 = 123;
  int v6 = 1010101;
  
  int retval;

  {
    TransactionGuard t1;
    tree.insert(s1, v1);
    tree.insert(s2, v2);
    tree.insert(s3, v3);
    tree.insert(s4, v4);
    tree.insert(s5, v5);
    tree.insert(s6, v6);
  }

  {
    TransactionGuard t2;
    assert(tree.lookup(s1, retval) && retval == v1);
    assert(tree.lookup(s2, retval) && retval == v2);
    assert(tree.lookup(s3, retval) && retval == v3);
    assert(tree.lookup(s4, retval) && retval == v4);
    assert(tree.lookup(s5, retval) && retval == v5);
    assert(tree.lookup(s6, retval) && retval == v6);
  }

  std::cout << "PASS: " << __FUNCTION__ << std::endl;
}

void testTwoRecordsReadingAndWritingEachOther() {
  ARTTree<int>* tree = new ARTTree<int>();
  std::string s1 = "string1";
  std::string s2 = "string2";
  
  int v1 = 92;
  int v2 = 41;
  
  int retval;

  {
    TransactionGuard t1;
    tree->insert(s1, v1);
    tree->insert(s2, v2);
  }
  
  {
    TestTransaction t2(2);
    tree->update(s1, v2);
    tree->lookup(s2, retval);

    TestTransaction t3(3);
    tree->update(s2, v1);
    tree->lookup(s1, retval);
    assert(t3.try_commit());
    assert(!t2.try_commit());
  }

  std::cout << "PASS: " << __FUNCTION__ << std::endl;
}

void testNewRecordFoundInAnotherTransaction() {
  ARTTree<int>* tree = new ARTTree<int>();
  std::string s1 = "testingstringone";
  int val1 = 39589320;
  
  int retval;

  {
    TestTransaction t1(1);
    tree->insert(s1, val1);

    TestTransaction  t2(2);
    try {
      tree->lookup(s1, retval);
      assert(false);
    } catch(Transaction::Abort e) {
      assert(t1.try_commit());
    }
    
    std::cout << "PASS: " << __FUNCTION__ << std::endl;
  }
}

void testCleanupCalledAfterDelete() {
  ARTTree<int>* tree = new ARTTree<int>();
  std::string s1 = "testingstringone";
  int val1 = 25;
  
  int retval;

  {
    TransactionGuard t1;
    tree->insert(s1, val1);
  }

  {
    TestTransaction t2(1);
    tree->remove(s1);
    
    TestTransaction t3(1);
    tree->lookup(s1, retval);
    assert(retval == val1);
    assert(t2.try_commit());
    assert(!tree->lookup(s1, retval));
    assert(!t3.try_commit());
    
  }

  std::cout << "PASS: " << __FUNCTION__ << std::endl;
}

void testAbortedInsert() {
  ARTTree<int>* tree = new ARTTree<int>();
  std::string s1 = "testingstringone";
  std::string s2 = "stringtwotest";
  
  int val1 = 190;
  int val2 = 20905;
  
  
  int retval;

  {
    TestTransaction t1(1);
    tree->insert(s1, val1);

    TestTransaction t2(1);
    tree->insert(s2, val2);
    try {
      tree->lookup(s1, retval);
      assert(false);
    } catch(Transaction::Abort e) {
      assert(!tree->lookup(s2, retval));
      assert(t1.try_commit());
    }
  }

  std::cout << "PASS: " << __FUNCTION__ << std::endl;
}

void testFindDeleteBitInRecordInAnotherTransaction() {
  ARTTree<int>* tree = new ARTTree<int>();
  std::string s1 = "testingstringone";
  
  int val1 = 190;

  int retval;

  {
    TransactionGuard t1;
    tree->insert(s1, val1);
  }
  
  {
    TestTransaction t1(1);
    tree->remove(s1);

    TestTransaction t2(2);
    assert(t1.try_commit());
    assert(!tree->lookup(s1, retval));
  }

  std::cout << "PASS: " << __FUNCTION__ << std::endl;
}

void testDeleteThenInsertSameTransaction() {
  ARTTree<int>* tree = new ARTTree<int>();
  std::string s1 = "testingstringone";
  
  int val1 = 190;
  int val2 = 200;

  int retval;

  {
    TransactionGuard t1;
    tree->insert(s1, val1);
  }
  
  {
    TransactionGuard t2;
    tree->remove(s1);
    tree->insert(s1, val2);
    assert(tree->lookup(s1, retval) && retval == val2);
  }

  {
    TransactionGuard t3;
    assert(tree->lookup(s1, retval) && retval == val2);
  }

  std::cout << "PASS: " << __FUNCTION__ << std::endl;
}

void testDeleteThenLookup() {
  ARTTree<int>* tree = new ARTTree<int>();
  std::string s1 = "testingstringone";
  
  int val1 = 190;

  int retval;

  {
    TransactionGuard t1;
    tree->insert(s1, val1);
  }

  {
    TransactionGuard t2;
    tree->remove(s1);
    assert(!tree->lookup(s1, retval));
  }

  {
    TransactionGuard t3;
    assert(!tree->lookup(s1, retval));
  }

  std::cout << "PASS: " << __FUNCTION__ << std::endl;
}

void testLookupThenDeleteDifferentTransactions() {
  ARTTree<int>* tree = new ARTTree<int>();
  std::string s1 = "testingstringone";
  int val1 = 190;
  int retval;

  {
    TransactionGuard t1;
    tree->insert(s1, val1);
  }

  {
    TestTransaction t2(1);
    tree->lookup(s1, retval);
    TestTransaction t3(2);
    tree->remove(s1);
    assert(t3.try_commit());
    assert(!t2.try_commit());
  }

  std::cout << "PASS: " << __FUNCTION__ << std::endl;
}

void testVarietyOperationsSameTransaction() {
  ARTTree<int>* tree = new ARTTree<int>();
  std::string s1 = "string2testing";
  int val1 = 190;
  int val2 = 1;
  int val3 = 20;
  
  int retval;

  {
    TestTransaction t2(1);
    tree->insert(s1, val2);
    assert(tree->lookup(s1, retval) && retval == val2);
    tree->remove(s1);
    assert(!tree->lookup(s1, retval));
    tree->insert(s1, val1);
    assert(tree->lookup(s1, retval) && retval == val1);
    tree->update(s1, val3);
    assert(tree->lookup(s1, retval) && retval == val3);
    tree->insert(s1, val2);
    assert(tree->lookup(s1, retval) && retval == val2);
    tree->remove(s1);
    assert(!tree->lookup(s1, retval));
    assert(t2.try_commit());
  }

  std::cout << "PASS: " << __FUNCTION__ << std::endl;
}

void testRemoveThenInsertMultipleTransactions() {
  ARTTree<int>* tree = new ARTTree<int>();
  std::string s1 = "string2testing";
  int val1 = 190;
  int val3 = 20;
  
  int retval;

  {
    TransactionGuard t1;
    tree->insert(s1, val1);
  }

  {
    TransactionGuard t2;
    tree->remove(s1);
  }

  {
    TransactionGuard t3;
    tree->insert(s1, val3);
  }

  {
    TransactionGuard t4;
    assert(tree->lookup(s1, retval) && retval == val3);
  }

  std::cout << "PASS: " << __FUNCTION__ << std::endl;
}

void testInsertThenDeleteInAnotherTransaction() {
  ARTTree<int>* tree = new ARTTree<int>();
  std::string s1 = "string2testing";
  int val1 = 190;
  int val2 = 20;
  
  int retval;

  {
    TransactionGuard t1;
    tree->insert(s1, val1);
  }
  
  {
    TestTransaction t1(1);
    tree->remove(s1);
    TestTransaction t2(2);
    tree->insert(s1, val2);
    assert(t2.try_commit());
    assert(t1.try_commit());
    TestTransaction t3(3);
    assert(!tree->lookup(s1, retval));
  }

  std::cout << "PASS: " << __FUNCTION__ << std::endl;
}

int main() {
  testSimpleString();
  testConcurrentInsertLookup();
  testPopulateTreeAndLookup();
  testTwoRecordsReadingAndWritingEachOther();
  testNewRecordFoundInAnotherTransaction();
  testCleanupCalledAfterDelete();
  testAbortedInsert();
  testFindDeleteBitInRecordInAnotherTransaction();
  testDeleteThenInsertSameTransaction();
  testDeleteThenLookup();
  testLookupThenDeleteDifferentTransactions();
  testVarietyOperationsSameTransaction();
  testRemoveThenInsertMultipleTransactions();
  testInsertThenDeleteInAnotherTransaction();
  return 0;
}
