#include <iostream>
#include <assert.h>

#include "Array.hh"
#include "Hashtable.hh"
#include "MassTrans.hh"
#include "Transaction.hh"

#define N 100

#define HASHTABLE 0

#if !HASHTABLE
kvepoch_t global_log_epoch = 0;
volatile uint64_t globalepoch = 1;     // global epoch, updated by main thread regularly
kvtimestamp_t initial_timestamp;
volatile bool recovering = false; // so don't add log entries, and free old value immediately
#endif

using namespace std;

void stringKeyTests() {
#if 1
  Hashtable<std::string, std::string> h;
  
  Transaction t;
  {
  std::string s1("bar");
  assert(h.transInsert(t, "foo", s1));
  }
  std::string s;
  assert(h.transGet(t, "foo", s));
  assert(s == "bar");
  assert(t.commit());

  Transaction t2;
  assert(h.transGet(t2, "foo", s));
  assert(s == "bar");
  assert(t2.commit());
#endif
}

void insertDeleteTest(bool shouldAbort) {
  MassTrans<int> h;
  Transaction t;
  for (int i = 10; i < 25; ++i) {
    assert(h.transInsert(t, i, i+1));
  }
  assert(t.commit());

  Transaction t2;
  assert(h.transInsert(t2, 25, 26));
  int x;
  assert(h.transGet(t2, 25, x));
  assert(!h.transGet(t2, 26, x));

  assert(h.transDelete(t2, 25));

  if (shouldAbort) {
    Transaction t3;
    assert(h.transInsert(t3, 26, 27));
    assert(t3.commit());

    try {
      t2.commit();
      assert(0);
    } catch (Transaction::Abort E) {}
  } else
    assert(t2.commit());
}

void insertDeleteSeparateTest() {
  MassTrans<int> h;
  Transaction t_init;
  for (int i = 10; i < 12; ++i) {
    assert(h.transInsert(t_init, i, i+1));
  }
  assert(t_init.commit());

  Transaction t;
  int x;
  assert(!h.transGet(t, 12, x));

  Transaction t2;
  assert(h.transInsert(t2, 12, 13));
  assert(h.transDelete(t2, 10));
  assert(t2.commit());
  
  try {
    t.commit();
    assert(0);
  } catch (Transaction::Abort E) {}


  Transaction t3;
  assert(!h.transGet(t3, 13, x));
  
  Transaction t4;
  assert(h.transInsert(t4, 10, 11));
  assert(h.transInsert(t4, 13, 14));
  assert(h.transDelete(t4, 11));
  assert(h.transDelete(t4, 12));
  assert(t4.commit());

  try {
    t3.commit();
    assert(0);
  } catch (Transaction::Abort E) {}

}

int main() {
  typedef int Key;
  typedef int Value;
#if HASHTABLE
  Hashtable<Key, Value> h;
#else
  MassTrans<Value> h;
  h.thread_init();
#endif

  Transaction t;

  Value v1,v2,vunused;
  //  assert(!h.transGet(t, 0, v1));

  assert(h.transInsert(t, 0, 1));
  h.transPut(t, 1, 3);
  
  assert(t.commit());

  Transaction tm;
  assert(h.transUpdate(tm, 1, 2));
  assert(tm.commit());

  Transaction t2;
  assert(h.transGet(t2, 1, v1));
  assert(t2.commit());

  Transaction t3;
  h.transPut(t3, 0, 4);
  assert(t3.commit());
  Transaction t4;
  assert(h.transGet(t4, 0, v2));
  assert(t4.commit());

  Transaction t5;
  assert(!h.transInsert(t5, 0, 5));
  assert(t5.commit());

  Transaction t6;
  assert(!h.transUpdate(t6, 2, 1));
  assert(t6.commit());

  Transaction t7;
  assert(!h.transGet(t7, 2, vunused));
  Transaction t8;
  assert(h.transInsert(t8, 2, 2));
  assert(t8.commit());

  try {
    t7.commit();
    assert(0);
  } catch(Transaction::Abort E) {}

  Transaction t9;
  assert(h.transInsert(t9, 3, 0));
  Transaction t10;
  assert(h.transInsert(t10, 4, 4));
  try{
    // t9 inserted invalid node, so we are forced to abort
    h.transUpdate(t10, 3, vunused);
    assert(0);
  } catch (Transaction::Abort E) {}
  Transaction t10_2;
  try {
    // deletes should also force abort from invalid nodes
    h.transDelete(t10_2, 3);
    assert(0);
  } catch (Transaction::Abort E) {}
  assert(t9.commit());
  assert(!t10.commit() && !t10_2.commit());

  Transaction t11;
  assert(h.transInsert(t11, 4, 5));
  assert(t11.commit());

  // basic delete
  Transaction t12;
  assert(!h.transDelete(t12, 5));
  assert(h.transUpdate(t12, 4, 0));
  assert(h.transDelete(t12, 4));
  assert(!h.transGet(t12, 4, vunused));
  assert(!h.transUpdate(t12, 4, 0));
  assert(!h.transDelete(t12, 4));
  assert(t12.commit());

  // delete-then-insert
  Transaction t13;
  assert(h.transGet(t13, 3, vunused));
  assert(h.transDelete(t13, 3));
  assert(h.transInsert(t13, 3, 1));
  assert(h.transGet(t13, 3, vunused));
  assert(t13.commit());

  // insert-then-delete
  Transaction t14;
  assert(!h.transGet(t14, 4, vunused));
  assert(h.transInsert(t14, 4, 14));
  assert(h.transGet(t14, 4, vunused));
  assert(h.transDelete(t14, 4));
  assert(!h.transGet(t14, 4, vunused));
  assert(t14.commit());

  // blind update success
  Transaction t15;
  assert(h.transUpdate(t15, 3, 15));
  Transaction t16;
  assert(h.transUpdate(t16, 3, 16));
  assert(t16.commit());
  assert(t15.commit());

  // update aborts after delete
  Transaction t17;
  Transaction t18;
  assert(h.transUpdate(t17, 3, 17));
  assert(h.transDelete(t18, 3));
  assert(t18.commit());
  try {
    t17.commit();
    assert(0);
  } catch (Transaction::Abort E) {}

  Transaction t19;
  h.transQuery(t19, "0", "4", [] (Masstree::Str s, int val) { printf("%s, %d\n", s.data(), val); });
  h.transQuery(t19, "4", "4", [] (Masstree::Str s, int val) { printf("%s, %d\n", s.data(), val); });
  assert(t19.commit());

  // insert-then-delete node test
  insertDeleteTest(false);
  insertDeleteTest(true);

  // insert-then-delete problems with masstree version numbers (currently fails)
  insertDeleteSeparateTest();

  h.print();

  cout << v1 << " " << v2 << endl;

  // string key testing
  stringKeyTests();
  
}
