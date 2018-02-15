#include <iostream>
#include <iomanip>
#include <sstream>
#include <cassert>

#include <chrono>
#include <thread>

#include "DB_index.hh"

// unit testing with key range 0000000-9999999
// scanner thread constantly asserts that the number of keys is multiple of 5
// and non-decreasing
// concurrent thread inserts keys to maintain this invariant

typedef MassTrans<std::string> mbta_type;

volatile mrcu_epoch_type active_epoch = 1;

void scanner(mbta_type* mbta) {
    TThread::set_id(0);
    mbta->thread_init();
    std::cout << "scanner started" << std::endl;
    while (true) {
        unsigned long cnt = 0;
        unsigned long cnt_snap;

        TRANSACTION {
            cnt_snap = cnt;
            mbta->transQuery("00000", "99999", [&] (Masstree::Str key, mbta_type::value_type& value) {
                assert(value == (std::string(key.s, key.len)+"v"));
                ++cnt_snap;
                return true;
            });
        } RETRY(true);

        assert(cnt_snap >= cnt);
        cnt = cnt_snap;

        if (cnt % 5) {
            std::stringstream ss;
            ss << "Invalid number of keys: " << cnt << std::endl;
            std::cerr << ss.str() << std::flush;
            //mbta->print_table();
            assert(0);
        }

        if (cnt > 999ul)
            break;
    }
}

void inserter(mbta_type* mbta) {
    TThread::set_id(1);
    mbta->thread_init();
    std::cout << "inserter started" << std::endl;
    unsigned long key = 0;
    while (true) {
        unsigned long key_snap;
        TRANSACTION {
            key_snap = key;
            for (int i = 0; i < 5; ++i) {
                std::stringstream ss;
                ss << std::setw(5) << std::setfill('0') << key_snap;
                mbta->transInsert(ss.str(), ss.str()+"v");
                ++key_snap;
                //std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        } RETRY(true);

        key = key_snap;

        if (key > 999ul)
            break;
    }
}

int main() {
    mbta_type mbta;

    std::thread scn(scanner, &mbta);
    std::thread ins(inserter, &mbta);
    scn.join();
    ins.join();
    std::cout << "Test pass." << std::endl;
    return 0;
}
