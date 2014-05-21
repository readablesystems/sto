
#define N 100

int main() {
  Array<int, N> a;

  transaction::start();
  ArrayElem e0 = a.at(0);
  ArrayElem e1 = a.at(1);
  
  void *c0 = e0.getContext();
  void *c1 = e1.getContext();

  auto v0 = e0.get();
  transaction::logRead(e0, c0);
  auto v1 = e1.get();
  e1.set(v0, c1);
  transaction::logReadWrite(e1, c1);

  if (v1 < v0) {
    e0.set(v1, c0);
    transaction::logWrite(e0);
  }

  transaction::commit();

}
