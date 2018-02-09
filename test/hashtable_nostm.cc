#define STO_NO_STM
#import "Hashtable.hh"

// tests of the hashtable without STM


template <typename MapType>
void basicTests(MapType& m) {
  assert(m.insert(1, "foo"));
  assert(!m.put(2, "bar"));

  std::string val;
  assert(m.read(1, val));
  assert(val == "foo");
  assert(m.read(2, val));
  assert(val == "bar");

  assert(m.put(1, "baz"));
  assert(m.remove(2));
  assert(!m.remove(2));

  assert(m.read(1, val));
  assert(val == "baz");
  assert(!m.read(2, val));

  // already present so should be a no op
  assert(!m.insert(1, "fuzz"));
  assert(m.read(1, val));
  assert(val == "baz");
}

int main() {
  Hashtable<int, std::string> h;
  basicTests(h);
}
