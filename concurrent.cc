#include <iostream>

#include "Array.hh"
#include "TransState.hh"

#define N 100

using namespace std;

int main() {
  Array<int, N> a;

  TransState t;

  auto v0 = a.transRead(t, 0);
  auto v1 = a.transRead(t, 1);

  a.transWrite(t, 0, 1<<0);
  a.transWrite(t, 1, 1<<1);

  t.commit();

  a.write(2, 1<<2);

  cout << a.read(0) << " " << a.read(1) << " " << a.read(2) << endl;
  
}
