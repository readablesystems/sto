#include "Transaction.hh"
#include "SingleElem.hh"
#include "TGeneric.hh"
#include <string>
#include <assert.h>

void bigObjTests() {
  struct foo {
    int a;
    int b;
    double c;
  };
  SingleElem<foo> f;
  printf("big obj size: %lu\n", sizeof(f));
  
  {
      TransactionGuard t;
      f = foo{1, 2, 3.4};
  }

  {
      TransactionGuard t2;
      foo f_read = f;
      assert(f_read.a == 1);
      assert(f_read.b == 2);
      assert(f_read.c - 3.4 < .001);
  }
}

void nontrivialObjTests() {
  SingleElem<std::string> f;
  printf("std string size: %lu\n", sizeof(f));

  {
      TransactionGuard t;
      f = "foobarbaz";
  }

  {
      TransactionGuard t2;
      std::string s = f;
      assert(s == std::string("foobarbaz"));
  }
}

float y = 1.1;
void genericSTMTests() {
  TGeneric stm;
  int x = 4;
  uint64_t *z = (uint64_t*)malloc(sizeof(*z));
  *z = 0xffffffffffULL;
  {TransactionGuard t;
    assert(stm.read(&x) == 4);
    stm.write(&x, 5);
    assert(stm.read(&x) == 5);
    
    assert(stm.read(&y) - 1.1 < 0.01);
    
    assert(stm.read(z) == (uint64_t)0xffffffffffULL);
    stm.write(z, (uint64_t)0x7777777777ULL);
    assert(stm.read(z) == (uint64_t)0x7777777777ULL);
  }

  assert(x == 5);

  {TransactionGuard t;
    assert(stm.read(&x) == 5);
    assert(stm.read(&y) - 1.1 < 0.01);
    assert(stm.read(z) == (uint64_t)0x7777777777ULL);
  }
}

void incrementTest() {
    SingleElem<int> f;
    
    TRANSACTION {
        f = 1;
    } RETRY(false);
    
    TRANSACTION {
        f = f + 1;
    } RETRY(false);
    
    int v;
    TRANSACTION {
        v = f;
    } RETRY(false);

    assert(v == 2);
}

int main() {
  SingleElem<int> f;
  printf("size: %lu\n", sizeof(f));

  {
      TransactionGuard t;
      f = 1;
      assert(f == 1);
  }

  {
      TransactionGuard t2;
      assert(f == 1);
  }

  bigObjTests();
  nontrivialObjTests();

  genericSTMTests();

  incrementTest();

  return 0;
}
