#include "Transaction.hh"
#include "SingleElem.hh"
#include "GenericSTM.hh"
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
  
  Transaction t;
  STO::set_transaction(&t);
  f.transWrite(foo{1, 2, 3.4});
  assert(t.try_commit());

  Transaction t2;
  STO::set_transaction(&t2);
  foo f_read = f.transRead();
  assert(f_read.a == 1);
  assert(f_read.b == 2);
  assert(f_read.c - 3.4 < .001);
  assert(t2.try_commit());
}

void nontrivialObjTests() {
  SingleElem<std::string> f;
  printf("std string size: %lu\n", sizeof(f));

  Transaction t;
  STO::set_transaction(&t);
  f.transWrite("foobarbaz");
  assert(t.try_commit());

  Transaction t2;
  STO::set_transaction(&t2);
  auto s = f.transRead();
  assert(s == std::string("foobarbaz"));
  assert(t2.try_commit());
}

float y = 1.1;
void genericSTMTests() {
  GenericSTM stm;
  int x = 4;
  uint64_t *z = (uint64_t*)malloc(sizeof(*z));
  *z = 0xffffffffffULL;
  {Transaction t;
    STO::set_transaction(&t);
    assert(stm.transRead(&x) == 4);
    stm.transWrite(&x, 5);
    assert(stm.transRead(&x) == 5);
    
    assert(stm.transRead(&y) - 1.1 < 0.01);
    
    assert(stm.transRead(z) == (uint64_t)0xffffffffffULL);
    stm.transWrite(z, (uint64_t)0x7777777777ULL);
    assert(stm.transRead(z) == (uint64_t)0x7777777777ULL);

    assert(t.try_commit());
  }

  assert(x == 5);

  {Transaction t;
    STO::set_transaction(&t);
    assert(stm.transRead(&x) == 5);
    assert(stm.transRead(&y) - 1.1 < 0.01);
    assert(stm.transRead(z) == (uint64_t)0x7777777777ULL);
    assert(t.try_commit());
  }
}

int main() {
  SingleElem<int> f;
  printf("size: %lu\n", sizeof(f));
  
  Transaction t;
  STO::set_transaction(&t);
  f.transWrite(1);
  assert(f.transRead() == 1);
  assert(t.try_commit());

  Transaction t2;
  STO::set_transaction(&t2);
  assert(f.transRead() == 1);
  assert(t2.try_commit());

  bigObjTests();
  nontrivialObjTests();

  genericSTMTests();

  return 0;
}
