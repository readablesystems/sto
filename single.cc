#include <iostream>
#include <assert.h>
#include <stdio.h>

#include "Hashtable.hh"
#include "MassTrans.hh"
#include "List.hh"
#include "Queue.hh"
#include "Transaction.hh"
#include "IntStr.hh"

#define N 100

volatile mrcu_epoch_type active_epoch = 1;

using namespace std;

template <typename T> class IntMassTrans {
    MassTrans<T> m_;
public:
    bool transGet(int k, T& v) {
        return m_.transGet(IntStr(k).str(), v);
    }
    bool transPut(int k, T v) {
        return m_.transPut(IntStr(k).str(), v);
    }
    bool transUpdate(int k, T v) {
        return m_.transUpdate(IntStr(k).str(), v);
    }
    bool transInsert(int k, T v) {
        return m_.transInsert(IntStr(k).str(), v);
    }
    bool transDelete(int k) {
        return m_.transDelete(IntStr(k).str());
    }
    void thread_init() {
        m_.thread_init();
    }
};

void queueTests() {
    Queue<int> q;
    int p;

    // NONEMPTY TESTS
    {
        // ensure pops read pushes in FIFO order
        TransactionGuard t;
        // q is empty
        q.transPush(1);
        q.transPush(2);
        assert(q.transFront(p) && p == 1); assert(q.transPop()); assert(q.transFront(p) && p == 2);
        assert(q.transPop());
    }

    {
        TransactionGuard t;
        // q is empty
        q.transPush(1);
        q.transPush(2);
    }

    {
        // front with no pops
        TransactionGuard t;
        assert(q.transFront(p));
        assert(p == 1);
        assert(q.transFront(p));
        assert(p == 1);
    }

    {
        // pop until empty
        TransactionGuard t;
        assert(q.transPop());
        assert(q.transPop());
        assert (!q.transPop());

        // prepare pushes for next test
        q.transPush(1);
        q.transPush(2);
        q.transPush(3);
    }

    {
        // fronts intermixed with pops
        TransactionGuard t;
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
    }

    {
        // front intermixed with pushes on nonempty
        TransactionGuard t;
        assert(q.transFront(p));
        assert(p == 1);
        assert(q.transFront(p));
        assert(p == 1);
        q.transPush(4);
        assert(q.transFront(p));
        assert(p == 1);
    }

    {
        // pops intermixed with pushes and front on nonempty
        // q = [1 2 3 4]
        TransactionGuard t;
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
    }

    // EMPTY TESTS
    {
        // front with empty queue
        TransactionGuard t;
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
    }

    {
        // pop with empty queue
        TransactionGuard t;
        // empty the queue
        assert(q.transPop());
        assert(!q.transPop());
       
        assert(!q.transFront(p));
       
        q.transPush(1);
        assert(q.transPop());
        assert(!q.transPop());
    }

    {
        // pop and front with empty queue
        TransactionGuard t;
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
    }

    // CONFLICTING TRANSACTIONS TEST
    {
        // test abortion due to pops 
        TestTransaction t1(1);
        // q has >1 element
        assert(q.transPop());
        TestTransaction t2(2);
        assert(q.transPop());
        assert(t1.try_commit());
        assert(!t2.try_commit());
    }

    {
        // test nonabortion T1 pops, T2 pushes on nonempty q
        TestTransaction t1(1);
        // q has >1 element
        assert(q.transPop());
        TestTransaction t2(2);
        q.transPush(3);
        assert(t1.try_commit());
        assert(t2.try_commit()); // commit should succeed 

    }
    // Nate: this used to be part of the above block but that doesn't make sense
    {
        TransactionGuard t1;
        assert(q.transFront(p) && p == 3);
        assert(q.transPop());
        assert(!q.transPop());
    }

    {
        // test abortion due to empty q pops
        TestTransaction t1(1);
        // q has 0 elements
        assert(!q.transPop());
        q.transPush(1);
        q.transPush(2);
        TestTransaction t2(2);
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
        TestTransaction t1(1);
        // q has 2 elements [1, 2]
        assert(q.transFront(p) && p == 1);
        q.transPush(4);

        // pop from non-empty q
        assert(q.transPop());
        assert(q.transFront(p));
        assert(p == 2);

        TestTransaction t2(2);
        q.transPush(3);
        // order of pushes doesn't matter, commits succeed
        assert(t2.try_commit());
        assert(t1.try_commit());

        // check if q is in order
        TestTransaction t(3);
        assert(q.transPop());
        assert(q.transFront(p));
        assert(p == 3);
        assert(q.transPop());
        assert(q.transFront(p));
        assert(p == 4);
        assert(q.transPop());
        assert(!q.transPop());
        assert(t.try_commit());
    }
}

void linkedListTests() {
  List<int> l;
  
  {
    TransactionGuard t;
    assert(!l.transFind(5));
    assert(l.transInsert(5));
    int *p = l.transFind(5);
    assert(*p == 5);
  }

  {
    TransactionGuard t;
    assert(!l.transInsert(5));
    assert(*l.transFind(5) == 5);
    assert(!l.transFind(7));
    assert(l.transInsert(7));
  }

  {
    TransactionGuard t;
    assert(l.size() == 2);
    assert(l.transInsert(10));
    assert(l.size() == 3);
    auto it = l.transIter();
    int i = 0;
    int elems[] = {5,7,10};
    while (it.transHasNext()) {
      assert(*it.transNext() == elems[i++]);
    }
  }


  {
    TransactionGuard t;
    assert(l.transDelete(7));
    assert(!l.transDelete(1000));
    assert(l.size() == 2);
    assert(!l.transFind(7));
    auto it = l.transIter();
    assert(*it.transNext() == 5);
    assert(*it.transNext() == 10);
  }

  {
    TransactionGuard t;
    assert(l.transInsert(7));
    assert(l.transDelete(7));
    assert(l.size() == 2);
    assert(!l.transFind(7));
  }
}

void stringKeyTests() {
#if 1
  Hashtable<std::string, std::string> h;
  std::string s;

  {
      TransactionGuard t;
      {
          std::string s1("bar");
          assert(h.transInsert("foo", s1));
      }
      assert(h.transGet("foo", s));
      assert(s == "bar");
  }

  {
      TransactionGuard t2;
      assert(h.transGet("foo", s));
      assert(s == "bar");
  }
#endif
}

void insertDeleteTest(bool shouldAbort) {
  MassTrans<int> h;
  {
      TransactionGuard t;
      for (int i = 10; i < 25; ++i) {
          assert(h.transInsert(IntStr(i).str(), i+1));
      }
  }

  TestTransaction t2(1);
  assert(h.transInsert(IntStr(25).str(), 26));
  int x;
  assert(h.transGet(IntStr(25).str(), x));
  assert(!h.transGet(IntStr(26).str(), x));

  assert(h.transDelete(IntStr(25).str()));

  if (shouldAbort) {
      TestTransaction t3(2);
      assert(h.transInsert(IntStr(26).str(), 27));
      assert(t3.try_commit());
      assert(!t2.try_commit());
  } else
      assert(t2.try_commit());
}

void insertDeleteSeparateTest() {
  MassTrans<int> h;
  {
      TransactionGuard t_init;
      for (int i = 10; i < 12; ++i) {
          assert(h.transInsert(IntStr(i).str(), i+1));
      }
  }

  TestTransaction t(1);
  int x;
  assert(!h.transGet(IntStr(12).str(), x));
  // need a write as well otherwise this txn would successfully commit as read-only
  h.transPut(IntStr(1000).str(), 0);

  TestTransaction t2(2);
  assert(h.transInsert(IntStr(12).str(), 13));
  assert(h.transDelete(IntStr(10).str()));
  assert(t2.try_commit());
  assert(!t.try_commit());


  TestTransaction t3(3);
  assert(!h.transGet(IntStr(13).str(), x));
  // need a write as well otherwise this txn would successfully commit as read-only
  h.transPut(IntStr(1000).str(), 0);

  TestTransaction t4(4);
  assert(h.transInsert(IntStr(10).str(), 11));
  assert(h.transInsert(IntStr(13).str(), 14));
  assert(h.transDelete(IntStr(11).str()));
  assert(h.transDelete(IntStr(12).str()));
  assert(t4.try_commit());
  assert(!t3.try_commit());
}

void rangeQueryTest() {
  MassTrans<int> h;
  int n = 99;
  char ns[64];

  {
      TransactionGuard t_init;
      sprintf(ns, "%d", n);
      for (int i = 10; i <= n; ++i) {
          assert(h.transInsert(IntStr(i).str(), i+1));
      }
  }

  {
  TransactionGuard t;
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
  }
}

template <typename K, typename V>
void basicQueryTests(MassTrans<K, V>& h) {
  TransactionGuard t19;
  h.transQuery("0", "2", [] (Masstree::Str s, int val) { printf("%s, %d\n", s.data(), val); return true; });
  h.transQuery("4", "4", [] (Masstree::Str s, int val) { printf("%s, %d\n", s.data(), val); return true; });
}

template <typename T>
void basicQueryTests(T&) {}

template <typename MapType>
void basicMapTests(MapType& h) {
  typedef int Value;
  Value v1,v2,vunused;

  {
      TransactionGuard t;
      //  assert(!h.transGet(t, 0, v1));
      assert(h.transInsert(0, 1));
      h.transPut(1, 3);
  }

  {
      TransactionGuard tm;
      assert(h.transUpdate(1, 2));
  }

  {
      TransactionGuard t2;
      assert(h.transGet(1, v1));
  }

  {
      TransactionGuard t3;
      h.transPut(0, 4);
  }
  {
      TransactionGuard t4;
      assert(h.transGet(0, v2));
  }

  {
      TransactionGuard t5;
      assert(!h.transInsert(0, 5));
  }

  {
      TransactionGuard t6;
      assert(!h.transUpdate(2, 1));
  }

  TestTransaction t7(1);
  assert(!h.transGet(2, vunused));
  // need a write as well otherwise this txn would successfully commit as read-only
  h.transPut(1000, 0);
  TestTransaction t8(2);
  assert(h.transInsert(2, 2));
  assert(t8.try_commit());

  assert(!t7.try_commit());

  TestTransaction t9(3);
  assert(h.transInsert(3, 0));
  TestTransaction t10(4);
  assert(h.transInsert(4, 4));
  try{
    // t9 inserted invalid node, so we are forced to abort
    h.transUpdate(3, vunused);
    assert(0);
  } catch (Transaction::Abort E) {}
  TestTransaction t10_2(5);
  try {
    // deletes should also force abort from invalid nodes
    h.transDelete(3);
    assert(0);
  } catch (Transaction::Abort E) {}
  assert(t9.try_commit());
  assert(!t10.try_commit() && !t10_2.try_commit());

  {
      TransactionGuard t11;
      assert(h.transInsert(4, 5));
  }

  // basic delete
  {
      TransactionGuard t12;
      assert(!h.transDelete(5));
      assert(h.transUpdate(4, 0));
      assert(h.transDelete(4));
      assert(!h.transGet(4, vunused));
      assert(!h.transUpdate(4, 0));
      assert(!h.transDelete(4));
  }

  // delete-then-insert
  {
      TransactionGuard t13;
      assert(h.transGet(3, vunused));
      assert(h.transDelete(3));
      assert(h.transInsert(3, 1));
      assert(h.transGet(3, vunused));
  }

  // insert-then-delete
  {
      TransactionGuard t14;
      assert(!h.transGet(4, vunused));
      assert(h.transInsert(4, 14));
      assert(h.transGet(4, vunused));
      assert(h.transDelete(4));
      assert(!h.transGet(4, vunused));
  }

  // blind update failure
  TestTransaction t15(1);
  assert(h.transUpdate(3, 15));
  TestTransaction t16(2);
  assert(h.transUpdate(3, 16));
  assert(t16.try_commit());
  // blind updates conflict each other now (not worth the extra trouble)
  assert(!t15.try_commit());


  // update aborts after delete
  TestTransaction t17(1);
  assert(h.transUpdate(3, 17));
  TestTransaction t18(2);
  assert(h.transDelete(3));
  assert(t18.try_commit());
  assert(!t17.try_commit());

  basicQueryTests(h);
}

int main() {

  // run on both Hashtable and MassTrans
  Hashtable<int, int> h;
  basicMapTests(h);
  IntMassTrans<int> m;
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
