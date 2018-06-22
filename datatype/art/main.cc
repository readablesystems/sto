#include "art.hh"

int main() {
    ART a;
    std::vector<uint8_t> key {1, 2, 3, 4};
    a.put(key, (void*) 123);
    auto n = (Node*) a.root.load();
    printf("%d\n", n->prefixLen);
    auto firstChild = (Leaf*) n->firstChild();
    printf("%lu\n", firstChild->value);

    // auto x = a.get(key);
    // printf("%lu %lu\n", (uintptr_t) x.first, (uintptr_t) x.second);
}
