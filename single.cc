#include <iostream>
#include <assert.h>

#include "Array.hh"
#include "Hashtable.hh"
#include "Transaction.hh"

#define N 100

using namespace std;

void stringKeyTests() {
#if 0
  Hashtable<std::string, std::string> h;
  Transaction t;
  assert(h.transInsert(t, "foo", "bar"));
  std::string s;
  assert(h.transGet(t, "foo", s));
  assert(t.commit());
#endif
}

int main() {
  typedef int Key;
  typedef int Value;
  Hashtable<Key, Value> h;

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
  

  h.print();

  cout << v1 << " " << v2 << endl;

  // string key testing
  stringKeyTests();
  
}
