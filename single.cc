#include <iostream>
#include <assert.h>
#include <stdio.h>

#include "Array.hh"
#include "Hashtable.hh"
#include "MassTrans.hh"
#include "List.hh"
#include "Queue.hh"
#include "Transaction.hh"

#define N 100

using namespace std;

void queueTests() {
    Queue<int> q;
    int p;

    // NONEMPTY TESTS
    {
        // ensure pops read pushes in FIFO order
        Transaction t;
        Sto::set_transaction(&t);
        // q is empty
        q.transPush(1);
        q.transPush(2);
        assert(q.transFront(p) && p == 1); assert(q.transPop()); assert(q.transFront(p) && p == 2);
        assert(q.transPop());
        assert(t.try_commit());
    }    
    
    {
        Transaction t;
        Sto::set_transaction(&t);
        // q is empty
        q.transPush(1);
        q.transPush(2);
        assert(t.try_commit());
    }

    {
        // front with no pops
        Transaction t;
        Sto::set_transaction(&t);
        assert(q.transFront(p));
        assert(p == 1);
        assert(q.transFront(p));
        assert(p == 1);
        assert(t.try_commit());
    }

    {
        // pop until empty
        Transaction t;
        Sto::set_transaction(&t);
        assert(q.transPop());
        assert(q.transPop());
        assert (!q.transPop());
        
        // prepare pushes for next test
        q.transPush(1);
        q.transPush(2);
        q.transPush(3);
        assert(t.try_commit());
    }

    {
        // fronts intermixed with pops
        Transaction t;
        Sto::set_transaction(&t);
        assert(q.transFront(p));
        assert(p == 1);
        assert(q.transPop());
        assert(q.transFront(p));
        assert(p == 2);
        assert(q.transPop());
        assert(q.transFront(p));
        assert(p == 3);
        assert(q.transPop());
        assert(!q.transPop());
        
        // set up for next test
        q.transPush(1);
        q.transPush(2);
        q.transPush(3);
        assert(t.try_commit());
    }

    {
        // front intermixed with pushes on nonempty
        Transaction t;
        Sto::set_transaction(&t);
        assert(q.transFront(p));
        assert(p == 1);
        assert(q.transFront(p));
        assert(p == 1);
        q.transPush(4);
        assert(q.transFront(p));
        assert(p == 1);
        assert(t.try_commit());
    }

    {
        // pops intermixed with pushes and front on nonempty
        // q = [1 2 3 4]
        Transaction t;
        Sto::set_transaction(&t);
        assert(q.transPop());
        assert(q.transFront(p));
        assert(p == 2);
        q.transPush(5);
        // q = [2 3 4 5]
        assert(q.transPop());
        assert(q.transFront(p));
        assert(p == 3);
        q.transPush(6);
        // q = [3 4 5 6]
        assert(t.try_commit());
    }

    // EMPTY TESTS
    {
        // front with empty queue
        Transaction t;
        Sto::set_transaction(&t);
        // empty the queue
        assert(q.transPop());
        assert(q.transPop());
        assert(q.transPop());
        assert(q.transPop());
        assert(!q.transPop());
        
        assert(!q.transFront(p));
       
        q.transPush(1);
        assert(q.transFront(p));
        assert(p == 1);
        assert(q.transFront(p));
        assert(p == 1);
        assert(t.try_commit());
    }

    {
        // pop with empty queue
        Transaction t;
        Sto::set_transaction(&t);
        // empty the queue
        assert(q.transPop());
        assert(!q.transPop());
       
        assert(!q.transFront(p));
       
        q.transPush(1);
        assert(q.transPop());
        assert(!q.transPop());
        assert(t.try_commit());
    }

    {
        // pop and front with empty queue
        Transaction t;
        Sto::set_transaction(&t);
        assert(!q.transFront(p));
       
        q.transPush(1);
        assert(q.transFront(p));
        assert(p == 1);
        assert(q.transPop());
       
        q.transPush(1);
        assert(q.transPop());
        assert(!q.transFront(p));
        assert(!q.transPop());

        // add items for next test
        q.transPush(1);
        q.transPush(2);

        assert(t.try_commit());
    }

    // CONFLICTING TRANSACTIONS TEST
    {
        // test abortion due to pops 
        Transaction t1;
        Transaction t2;
        // q has >1 element
        Sto::set_transaction(&t1);
        assert(q.transPop());
        Sto::set_transaction(&t2);
        assert(q.transPop());
        assert(t1.try_commit());
        assert(!t2.try_commit());
    }

    {
        // test nonabortion T1 pops, T2 pushes on nonempty q
        Transaction t1;
        Transaction t2;
        // q has >1 element
        Sto::set_transaction(&t1);
        assert(q.transPop());
        Sto::set_transaction(&t2);
        q.transPush(3);
        assert(t1.try_commit());
        assert(t2.try_commit()); // commit should succeed 

    }
    // Nate: this used to be part of the above block but that doesn't make sense
    {
        Transaction t1;
        Sto::set_transaction(&t1);
        assert(q.transFront(p) && p == 3);
        assert(q.transPop());
        assert(!q.transPop());
        assert(t1.try_commit());
    }

    {
        // test abortion due to empty q pops
        Transaction t1;
        Transaction t2;
        // q has 0 elements
        Sto::set_transaction(&t1);
        assert(!q.transPop());
        q.transPush(1);
        q.transPush(2);
        Sto::set_transaction(&t2);
        q.transPush(3);
        q.transPush(4);
        q.transPush(5);
        
        // read-my-write, lock tail
        assert(q.transPop());
        
        assert(t1.try_commit());
        assert(!t2.try_commit());
    }

    {
        // test nonabortion T1 pops/fronts and pushes, T2 pushes on nonempty q
        Transaction t1;
        Transaction t2;
        
        // q has 2 elements [1, 2]
        Sto::set_transaction(&t1);
        assert(q.transFront(p) && p == 1);
        q.transPush(4);

        // pop from non-empty q
        assert(q.transPop());
        assert(q.transFront(p));
        assert(p == 2);

        Sto::set_transaction(&t2);
        q.transPush(3);
        // order of pushes doesn't matter, commits succeed
        assert(t2.try_commit());
        assert(t1.try_commit());

        // check if q is in order
        Transaction t;
        Sto::set_transaction(&t);
        assert(q.transPop());
        assert(q.transFront(p));
        assert(p == 3);
        assert(q.transPop());
        assert(q.transFront(p));
        assert(p == 4);
        assert(q.transPop());
        assert(!q.transPop());
        assert(t1.try_commit());
    }
}

void linkedListTests() {
  List<int> l;
  
  {
    Transaction t;
    Sto::set_transaction(&t);
    assert(!l.transFind(5));
    assert(l.transInsert(5));
    int *p = l.transFind(5);
    assert(*p == 5);
    assert(t.try_commit());
  }

  {
    Transaction t;
    Sto::set_transaction(&t);
    assert(!l.transInsert(5));
    assert(*l.transFind(5) == 5);
    assert(!l.transFind(7));
    assert(l.transInsert(7));
    assert(t.try_commit());
  }

  {
    Transaction t;
    Sto::set_transaction(&t);
    assert(l.transSize() == 2);
    assert(l.transInsert(10));
    assert(l.transSize() == 3);
    auto it = l.transIter();
    int i = 0;
    int elems[] = {5,7,10};
    while (it.transHasNext()) {
      assert(*it.transNext() == elems[i++]);
    }
    assert(t.try_commit());
  }


  {
    Transaction t;
    Sto::set_transaction(&t);
    assert(l.transDelete(7));
    assert(!l.transDelete(1000));
    assert(l.transSize() == 2);
    assert(!l.transFind(7));
    auto it = l.transIter();
    assert(*it.transNext() == 5);
    assert(*it.transNext() == 10);
    assert(t.try_commit());
  }

  {
    Transaction t;
    Sto::set_transaction(&t);
    assert(l.transInsert(7));
    assert(l.transDelete(7));
    assert(l.transSize() == 2);
    assert(!l.transFind(7));
    assert(t.try_commit());
  }

}

void stringKeyTests() {
#if 1
  Hashtable<std::string, std::string> h;
  
  Transaction t;
  Sto::set_transaction(&t);
  {
  std::string s1("bar");
  assert(h.transInsert("foo", s1));
  }
  std::string s;
  assert(h.transGet("foo", s));
  assert(s == "bar");
  assert(t.try_commit());

  Transaction t2;
  Sto::set_transaction(&t2);
  assert(h.transGet("foo", s));
  assert(s == "bar");
  assert(t2.try_commit());
#endif
}

void insertDeleteTest(bool shouldAbort) {
  MassTrans<int> h;
  Transaction t;
  Sto::set_transaction(&t);
  for (int i = 10; i < 25; ++i) {
    assert(h.transInsert(i, i+1));
  }
  assert(t.try_commit());

  Transaction t2;
  Sto::set_transaction(&t2);
  assert(h.transInsert(25, 26));
  int x;
  assert(h.transGet(25, x));
  assert(!h.transGet(26, x));

  assert(h.transDelete(25));

  if (shouldAbort) {
    Transaction t3;
    Sto::set_transaction(&t3);
    assert(h.transInsert(26, 27));
    assert(t3.try_commit());
    assert(!t2.try_commit());
  } else
    assert(t2.try_commit());
}

void insertDeleteSeparateTest() {
  MassTrans<int> h;
  Transaction t_init;
  Sto::set_transaction(&t_init);
  for (int i = 10; i < 12; ++i) {
    assert(h.transInsert(i, i+1));
  }
  assert(t_init.try_commit());

  Transaction t;
  Sto::set_transaction(&t);
  int x;
  assert(!h.transGet(12, x));

  Transaction t2;
  Sto::set_transaction(&t2);
  assert(h.transInsert(12, 13));
  assert(h.transDelete(10));
  assert(t2.try_commit());
  assert(!t.try_commit());


  Transaction t3;
  Sto::set_transaction(&t3);
  assert(!h.transGet(13, x));
  
  Transaction t4;
  Sto::set_transaction(&t4);
  assert(h.transInsert(10, 11));
  assert(h.transInsert(13, 14));
  assert(h.transDelete(11));
  assert(h.transDelete(12));
  assert(t4.try_commit());
  assert(!t3.try_commit());

}

void rangeQueryTest() {
  MassTrans<int> h;
  
  Transaction t_init;
  Sto::set_transaction(&t_init);
  int n = 99;
  char ns[64];
  sprintf(ns, "%d", n);
  for (int i = 10; i <= n; ++i) {
    assert(h.transInsert(i, i+1));
  }
  assert(t_init.try_commit());

  Transaction t;
  Sto::set_transaction(&t);
  int x = 0;
  h.transQuery("10", Masstree::Str(), [&] (Masstree::Str , int ) { x++; return true; });
  assert(x == n-10+1);
  
  x = 0;
  h.transQuery("10", ns, [&] (Masstree::Str , int) { x++; return true; });
  assert(x == n-10);

  x = 0;
  h.transRQuery(ns, Masstree::Str(), [&] (Masstree::Str , int ) { x++; return true; });
  assert(x == n-10+1);
  
  x = 0;
  h.transRQuery(ns, "90", [&] (Masstree::Str , int ) { x++; return true; });
  assert(x == n-90);

  x = 0;
  h.transQuery("10", "25", [&] (Masstree::Str , int ) { x++; return true; });
  assert(x == 25-10);

  x = 0;
  h.transQuery("10", "26", [&] (Masstree::Str , int ) { x++; return true; });
  assert(x == 26-10);

  assert(t.try_commit());
}

template <typename K, typename V>
void basicQueryTests(MassTrans<K, V>& h) {
  Transaction t19;
  Sto::set_transaction(&t19);
  h.transQuery("0", "2", [] (Masstree::Str s, int val) { printf("%s, %d\n", s.data(), val); return true; });
  h.transQuery("4", "4", [] (Masstree::Str s, int val) { printf("%s, %d\n", s.data(), val); return true; });
  assert(t19.try_commit());
}

template <typename T>
void basicQueryTests(T&) {}

template <typename MapType>
void basicMapTests(MapType& h) {
  typedef int Key;
  typedef int Value;

  Transaction t;
  Sto::set_transaction(&t);

  Value v1,v2,vunused;
  //  assert(!h.transGet(t, 0, v1));

  assert(h.transInsert(0, 1));
  h.transPut(1, 3);
  
  assert(t.try_commit());

  Transaction tm;
  Sto::set_transaction(&tm);
  assert(h.transUpdate(1, 2));
  assert(tm.try_commit());

  Transaction t2;
  Sto::set_transaction(&t2);
  assert(h.transGet(1, v1));
  assert(t2.try_commit());

  Transaction t3;
  Sto::set_transaction(&t3);
  h.transPut(0, 4);
  assert(t3.try_commit());
  Transaction t4;
  Sto::set_transaction(&t4);
  assert(h.transGet(0, v2));
  assert(t4.try_commit());

  Transaction t5;
  Sto::set_transaction(&t5);
  assert(!h.transInsert(0, 5));
  assert(t5.try_commit());

  Transaction t6;
  Sto::set_transaction(&t6);
  assert(!h.transUpdate(2, 1));
  assert(t6.try_commit());

  Transaction t7;
  Sto::set_transaction(&t7);
  assert(!h.transGet(2, vunused));
  Transaction t8;
  Sto::set_transaction(&t8);
  assert(h.transInsert(2, 2));
  assert(t8.try_commit());

  assert(!t7.try_commit());

  Transaction t9;
  Sto::set_transaction(&t9);
  assert(h.transInsert(3, 0));
  Transaction t10;
  Sto::set_transaction(&t10);
  assert(h.transInsert(4, 4));
  try{
    // t9 inserted invalid node, so we are forced to abort
    h.transUpdate(3, vunused);
    assert(0);
  } catch (Transaction::Abort E) {}
  Transaction t10_2;
  Sto::set_transaction(&t10_2);
  try {
    // deletes should also force abort from invalid nodes
    h.transDelete(3);
    assert(0);
  } catch (Transaction::Abort E) {}
  assert(t9.try_commit());
  assert(!t10.try_commit() && !t10_2.try_commit());

  Transaction t11;
  Sto::set_transaction(&t11);
  assert(h.transInsert(4, 5));
  assert(t11.try_commit());

  // basic delete
  Transaction t12;
  Sto::set_transaction(&t12);
  assert(!h.transDelete(5));
  assert(h.transUpdate(4, 0));
  assert(h.transDelete(4));
  assert(!h.transGet(4, vunused));
  assert(!h.transUpdate(4, 0));
  assert(!h.transDelete(4));
  assert(t12.try_commit());

  // delete-then-insert
  Transaction t13;
  Sto::set_transaction(&t13);
  assert(h.transGet(3, vunused));
  assert(h.transDelete(3));
  assert(h.transInsert(3, 1));
  assert(h.transGet(3, vunused));
  assert(t13.try_commit());

  // insert-then-delete
  Transaction t14;
  Sto::set_transaction(&t14);
  assert(!h.transGet(4, vunused));
  assert(h.transInsert(4, 14));
  assert(h.transGet(4, vunused));
  assert(h.transDelete(4));
  assert(!h.transGet(4, vunused));
  assert(t14.try_commit());

  // blind update failure
  Transaction t15;
  Sto::set_transaction(&t15);
  assert(h.transUpdate(3, 15));
  Transaction t16;
  Sto::set_transaction(&t16);
  assert(h.transUpdate(3, 16));
  assert(t16.try_commit());
  // blind updates conflict each other now (not worth the extra trouble)
  assert(!t15.try_commit());


  // update aborts after delete
  Transaction t17;
  Transaction t18;
  Sto::set_transaction(&t17);
  assert(h.transUpdate(3, 17));
  Sto::set_transaction(&t18);
  assert(h.transDelete(3));
  assert(t18.try_commit());
  assert(!t17.try_commit());

  basicQueryTests(h);
}

int main() {

  // run on both Hashtable and MassTrans
  Hashtable<int, int> h;
  basicMapTests(h);
  MassTrans<int> m;
  m.thread_init();
  basicMapTests(m);

  // insert-then-delete node test
  insertDeleteTest(false);
  insertDeleteTest(true);

  // insert-then-delete problems with masstree version numbers (currently fails)
  insertDeleteSeparateTest();

  rangeQueryTest();

  // string key testing
  stringKeyTests();

  linkedListTests();
  
  queueTests();
}
