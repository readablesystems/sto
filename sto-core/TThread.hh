#pragma once

#include <cassert>
#include <random>

class Transaction;

struct PercentGen {
    //std::random_device rd;
    std::mt19937 gen;
    std::uniform_int_distribution<int> dist;

    PercentGen() : gen(0), dist(0,99) {}
    ~PercentGen() {}

    int sample() {
        return dist(gen);
    }
    bool chance(int percent) {
        return (sample() < percent);
    }
};

class TThread {
    static __thread int the_id;
    static __thread bool always_allocate_;
    static __thread int hashsize_;
public:
    static __thread Transaction* txn;

// #ifdef TWO_PHASE_TRANSACTION
//     // Used to check the status of a Two-Phase Transaction, if one is in
//     // progress.
//     static __thread bool two_phase_transaction_;
//     static __thread bool second_phase_;
// #endif
    static PercentGen gen[];

    static int id() {
        return the_id;
    }
    static void set_id(int id) {
        assert(id >= 0 && id < 128);
        the_id = id;
    }
    static bool always_allocate() {
        return always_allocate_;
    }
    static void set_always_allocate(bool always_allocate) {
        always_allocate_ = always_allocate;
    }
    static int hashsize() {
        return hashsize_;
    }
    static void set_hashsize(int hashsize) {
        hashsize_ = hashsize;
    }
};
