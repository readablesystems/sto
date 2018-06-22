#include "art.hh"

int main() {
    ART a;
    std::vector<uint8_t> key1 {1, 2, 3, 4};
    std::vector<uint8_t> key2 {2, 2, 3, 5, 7};

    uintptr_t val1 = 123;
    uintptr_t val2 = 321;

    a.put(key1, (void*) val1);
    a.put(key2, (void*) val2);
    // auto n = (Node*) a.root.load();
    // printf("%d\n", n->prefixLen);
    // auto firstChild = (Leaf*) n->firstChild();
    // printf("%lu\n", firstChild->value);

    auto x = a.get(key1);
    auto y = a.get(key2);

    printf("%lu %lu\n", x.first, y.first);
    // printf("%lu %lu\n", (uintptr_t) x.first, (uintptr_t) x.second);
}
