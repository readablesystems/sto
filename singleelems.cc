#include "Transaction.hh"
#include "SingleElem.hh"
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

  return 0;
}
