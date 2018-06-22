#include "art.hh"

int main() {
    ART a;
    std::vector<uint8_t> key {1, 2, 3, 4};
    a.put(key, (void*) 123);
}
