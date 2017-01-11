#include "sampling.hh"
#include <iostream>

int main() {
    StoRandomDistribution *dist = new StoZipfDistribution(1000);
    dist->generate();
    for (int i = 0; i < 100; ++i)
        std::cout << dist->sample() << ", ";
    std::cout << std::endl;

    delete dist;
}
