#include "sampling.hh"
#include <iostream>

using namespace StoSampling;

int main() {
    StoRandomDistribution *dist = new StoZipfDistribution(1000);

    for (int i = 0; i < 1000; ++i)
        std::cout << dist->sample() << ", ";
    std::cout << std::endl;

    delete dist;

    return 0;
}
