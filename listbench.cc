#define LISTBENCH 1

#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <cstdlib>
#include <cassert>
#include <getopt.h>
#include "listbench.hh"
#include "ListS.hh"

#define MAX_ELEMENTS 4096
using list_type = List<int, int>;
uint64_t bm_ctrs[4];
std::vector<TransactionTid::type> snapshot_ids;

static inline void print_progress(size_t i) {
    if (i % 4096 == 0) {
        std::cout << "begin txn " << i << std::endl;
    }
}

static inline void reset_counters() {
    bzero(bm_ctrs, sizeof(bm_ctrs));
}

void random_populate(list_type* l, size_t ntxn, size_t max_txn_len) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(1, MAX_ELEMENTS);
    for (size_t i = 0; i < ntxn; ++i) {
        print_progress(i);
        size_t txnlen = ((size_t)dis(gen)) % max_txn_len + 1;
        TRANSACTION {
            for (size_t j = 0; j < txnlen; ++j) {
                (*l)[dis(gen)] = dis(gen);
            }
        } RETRY(false);
    }
}

void random_search(list_type* l, size_t ntxn, size_t max_txn_len) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(1, MAX_ELEMENTS);

    for (size_t i = 0; i < ntxn; ++i) {
        print_progress(i);
        size_t txnlen = ((size_t)dis(gen)) % max_txn_len + 1;
        TRANSACTION {
            for (size_t j = 0; j < txnlen; ++j) {
                auto p = l->trans_find(dis(gen));
                assert(!p.first || p.second <= MAX_ELEMENTS);
            }
        } RETRY(false);
    }
}

void random_populate_snapshot(list_type* l, size_t ntxn, size_t max_txn_len, int snap_factor) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(1, MAX_ELEMENTS);
    int snapshot_threshold = MAX_ELEMENTS / snap_factor;
    for (size_t i = 0; i < ntxn; ++i) {
        print_progress(i);
        size_t txnlen = ((size_t)dis(gen)) % max_txn_len + 1;
        TRANSACTION {
            for (size_t j = 0; j < txnlen; ++j)
                (*l)[dis(gen)] = dis(gen);
        } RETRY(false);

        if (snapshot_ids.empty() || dis(gen) <= snapshot_threshold) {
            bm_ctrs[snapshots_taken]++;
            snapshot_ids.push_back(Sto::take_snapshot());
        }
    }
}

void random_search_snapshot(list_type* l, size_t ntxn, size_t max_txn_len) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(1, MAX_ELEMENTS);
    assert(!snapshot_ids.empty());
    TransactionTid::type sid = snapshot_ids.at(snapshot_ids.size()/2);
    for (size_t i = 0; i < ntxn; ++i) {
        print_progress(i);
        size_t txnlen = ((size_t)dis(gen)) % max_txn_len + 1;
        TRANSACTION {
            Sto::set_active_sid(sid);
            for (size_t j = 0; j < txnlen; ++j) {
                auto p = l->trans_find(dis(gen));
                assert(!p.first || p.second <= MAX_ELEMENTS);
            }
        } RETRY(false);
    }
}

#define TEST_INS       0
#define TEST_INS_SNAP  1
#define TEST_FIND      2
#define TEST_FIND_SNAP 3

static int test_no = 0;
static int snap_factor = 2;
static size_t ntxns = 32768;
static size_t max_txn_len = 15;

static const std::string test_names[4] = {
    "random-insert",
    "random-insert-snapshot",
    "random-lookup",
    "random-lookup-snapshot"
};

void print_info() {
    std::cout << "Running test    = " << test_names[test_no] << std::endl;
    std::cout << "Number of TXs   = " << ntxns << std::endl;
    std::cout << "Maximum TX len  = " << max_txn_len << std::endl;
    if (test_no & 1) {
        std::cout << "Snapshot factor = " << snap_factor << std::endl;
    }
}

int main(int argc, char** argv) {
    /* The getopt loop, mostly based on example from GNU libc manual */
    while (true) {
        static struct option long_options[] =
        {
            /* These options set a flag. */
            {"insert",          no_argument, &test_no, TEST_INS},
            {"insert-snapshot", no_argument, &test_no, TEST_INS_SNAP},
            {"search",          no_argument, &test_no, TEST_FIND},
            {"search-snapshot", no_argument, &test_no, TEST_FIND_SNAP},
            /* These options don’t set a flag.
            We distinguish them by their indices. */
            {"num-txns",    required_argument, 0, 'n'},
            {"max-txlen",   required_argument, 0, 't'},
            {"snap-factor", required_argument, 0, 's'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        int c = getopt_long (argc, argv, "n:t:s:",
                        long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c) {
        case 0:
            /* If this option set a flag, do nothing else now. */
            if (long_options[option_index].flag != 0)
                break;
            std::cerr << "Unkown option " << long_options[option_index].name;
            if (optarg)
                std::cerr << " (with argument " << optarg << ")";
            std::cerr << std::endl;
            break;

        case 'n':
            ntxns = strtoul(optarg, nullptr, 10);
            break;

        case 't':
            max_txn_len = strtoul(optarg, nullptr, 10);
            break;

        case 's':
            snap_factor = atoi(optarg);
            break;

        case '?':
            /* getopt_long already printed an error message. */
            break;

        default:
            abort();
        }
    }

    print_info();

    list_type l;
    std::cout << "prepopulating list..." << std::endl;
    if (test_no == TEST_FIND || test_no == TEST_FIND_SNAP) {
        random_populate_snapshot(&l, ntxns, max_txn_len, snap_factor);
    } else {
        random_populate(&l, ntxns, max_txn_len);
    }

    reset_counters();
    std::cout << "starting test..." << std::endl;

    auto start = std::chrono::system_clock::now();
    if (test_no == TEST_INS)
        random_populate(&l, ntxns, max_txn_len);
    else if (test_no == TEST_INS_SNAP)
        random_populate_snapshot(&l, ntxns, max_txn_len, snap_factor);
    else if (test_no == TEST_FIND)
        random_search(&l, ntxns, max_txn_len);
    else if (test_no == TEST_FIND_SNAP)
        random_search_snapshot(&l, ntxns, max_txn_len);
    else
        abort();

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "time elapsed: " << elapsed.count() << " ms" << std::endl;
    std::cout << "base visited = " << bm_ctrs[base_visited]
            << ", histories searched = " << bm_ctrs[histories_searched] << std::endl;
    std::cout << "snapshot taken = " << bm_ctrs[snapshots_taken]
            << ", copy-on-writes = " << bm_ctrs[cow_performed] << std::endl;
    double top_spd = (double)bm_ctrs[base_visited] / (double)elapsed.count() * 1000.0;
    std::cout << "speed: " << top_spd << " elements/sec" << std::endl;
    return 0;
}
