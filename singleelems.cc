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
  f.transWrite(t, foo{1, 2, 3.4});
  assert(t.commit());

  Transaction t2;
  foo f_read = f.transRead(t2);
  assert(f_read.a == 1);
  assert(f_read.b == 2);
  assert(f_read.c - 3.4 < .001);
  assert(t2.commit());
}

void nontrivialObjTests() {
  SingleElem<std::string> f;
  printf("std string size: %lu\n", sizeof(f));

  Transaction t;
  f.transWrite(t, "foobarbaz");
  assert(t.commit());

  Transaction t2;
  auto s = f.transRead(t2);
  assert(s == std::string("foobarbaz"));
  assert(t2.commit());
}

float y = 1.1;
void genericSTMTests() {
  GenericSTM<uint32_t> stm4;
  GenericSTM<uint64_t> stm8;
  int x = 4;
  uint64_t *z = (uint64_t*)malloc(sizeof(*z));
  *z = 0xffffffffffULL;
  {Transaction t;
    assert(stm4.transRead(t, &x) == 4);
    stm4.transWrite(t, &x, 5);
    assert(stm4.transRead(t, &x) == 5);
    
    assert(stm4.transRead(t, &y) - 1.1 < 0.01);
    
    assert(stm8.transRead(t, z) == 0xffffffffffULL);
    stm8.transWrite(t, z, 0x7777777777ULL);
    assert(stm8.transRead(t, z) == 0x7777777777ULL);

    assert(t.commit());
  }

  assert(x == 5);

  {Transaction t;
    assert(stm4.transRead(t, &x) == 5);
    assert(stm4.transRead(t, &y) - 1.1 < 0.01);
    assert(stm8.transRead(t, z) == 0x7777777777ULL);
    assert(t.commit());
  }
}

int main() {
  SingleElem<int> f;
  printf("size: %lu\n", sizeof(f));
  
  Transaction t;
  f.transWrite(t, 1);
  assert(f.transRead(t) == 1);
  assert(t.commit());

  Transaction t2;
  assert(f.transRead(t2) == 1);
  assert(t2.commit());

  bigObjTests();
  nontrivialObjTests();

  genericSTMTests();

  return 0;
}
