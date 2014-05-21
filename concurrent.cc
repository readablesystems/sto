#include "Array.hh"
#include "TransState.hh"

#define N 100

int main() {
  Array<int, N> a;

  TransState t;

  auto v0 = a.transRead(t, 0);
  auto v1 = a.transRead(t, 1);

  if (v0 < v1) {
    a.transWrite(t, 1, v0);
  } else {
    a.transWrite(t, 0, v1);
  }

  t.commit();
  
}
