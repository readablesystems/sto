#include <iostream>
#include "sampling.hh"

using namespace sampling;

int main() {
    // testing a zipf distribution, generating numbers between 1 and 1000 with theta=0.8
    StoRandomDistribution<>::rng_type rng(1/*seed*/);
    StoRandomDistribution<> *dist = new StoZipfDistribution<>(rng /*generator*/, 1 /*low*/, 1000 /*high*/, 0.8 /*skew*/);

    for (int i = 0; i < 1000; ++i)
        std::cout << dist->sample() << ", ";
    std::cout << std::endl;

    delete dist;

    return 0;
}
