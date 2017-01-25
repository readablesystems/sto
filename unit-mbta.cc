#include <iostream>
#include <iomanip>
#include <sstream>
#include "MassTrans.hh"
#include <cassert>

#include <chrono>
#include <thread>

// unit testing with key range 0000000-9999999
// scanner thread constantly asserts that the number of keys is multiple of 5
// and non-decreasing
// concurrent thread inserts keys to maintain this invariant

typedef MassTrans<std::string> mbta_type;

void scanner(mbta_type& mbta) {
    std::cout << "scanner started" << std::endl;
    while (true) {
        unsigned long cnt = 0;
        TRANSACTION {
            mbta.transQuery("0000000", "9999999", [&] (Masstree::Str key, mbta_type::value_type& value) {
                assert(value == (std::string(key.s, key.len)+"v"));
                ++cnt;
                return true;
            });
        } RETRY(true);

        if (cnt % 5) {
            std::stringstream ss;
            ss << "Invalid number of keys: " << cnt << std::endl;
            std::cerr << ss.str() << std::flush;
            //mbta.print_table();
            assert(0);
        }

        if (cnt > 9999999ul)
            break;
    }
}

void inserter(mbta_type& mbta) {
    std::cout << "inserter started" << std::endl;
    unsigned long key = 0;
    while (true) {
        TRANSACTION {
            for (int i = 0; i < 5; ++i) {
                std::stringstream ss;
                ss << std::setw(7) << std::setfill('0') << key;
                mbta.transInsert(ss.str(), ss.str()+"v");
                ++key;
                //std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        } RETRY(true);

        if (key > 9999999ul)
            break;
    }
}

int main() {
    mbta_type mbta;

    std::thread scn(scanner, std::ref(mbta));
    std::thread ins(inserter, std::ref(mbta));
    scn.join();
    ins.join();
    std::cout << "Test pass." << std::endl;
    return 0;
}
