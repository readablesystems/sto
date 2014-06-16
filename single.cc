#include <iostream>
#include <assert.h>

#include "Array.hh"
#include "Hashtable.hh"
#include "Transaction.hh"

#define N 100

using namespace std;

int main() {
  typedef int Key;
  typedef int Value;
  Hashtable<Key, Value> h;

  Transaction t;

  Value v1,v2,v3;
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
  assert(!h.transGet(t7, 2, v3));
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
    h.transUpdate(t10, 3, v3);
    assert(0);
  } catch (Transaction::Abort E) {}
  assert(t9.commit());
  assert(!t10.commit());
  Transaction t11;
  assert(h.transInsert(t11, 4, 5));
  assert(t11.commit());

  h.print();

  cout << v1 << " " << v2 << endl;
  
}
