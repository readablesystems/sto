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

  Value v1,v2;
    assert(!h.transGet(t, 0, v1));

    assert(h.transInsert(t, 0, 1));
    h.transPut(t, 1, 3);
  
  assert(t.commit());

  Transaction tm;
  assert(h.transSet(tm, 1, 2));
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
  assert(!h.transSet(t6, 2, 1));
  assert(t6.commit());

  cout << v1 << " " << v2 << endl;
  
}
