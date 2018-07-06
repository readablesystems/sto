#undef NDEBUG
#include <fstream>
#include <string>
#include <thread>
#include <iostream>
#include <cassert>
#include <vector>
#include "Sto.hh"
#include "TART.hh"
#include "Transaction.hh"
#include <unistd.h>

#define NTHREAD 10
#define NVALS 1000000

TART art;

void printMem() {
    int tSize = 0, resident = 0, share = 0;
    std::ifstream buffer("/proc/self/statm");
    buffer >> tSize >> resident >> share;
    buffer.close();

    long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024; // in case x86-64 is configured to use 2MB pages
    double rss = resident * page_size_kb;
    std::cout << "Memory - " << rss << " kB\n";
}

std::vector<unsigned char> intToBytes(int paramInt)
{
    std::vector<unsigned char> arrayOfByte(4);
     for (int i = 0; i < 4; i++)
         arrayOfByte[3 - i] = (paramInt >> (i * 8));
     return arrayOfByte;
}

void insertKey(int thread_id) {
    TThread::set_id(thread_id);

    for (int i = thread_id*(NVALS/NTHREAD); i < (thread_id+1)*NVALS/NTHREAD; i++) {
        auto v = intToBytes(i);
        std::string str(v.begin(),v.end());
        TRANSACTION_E {
            art.insert(str, i);
        } RETRY_E(true);
    }
}

void lookupKey(int thread_id) {
    TThread::set_id(thread_id);

    for (int i = thread_id*(NVALS/NTHREAD); i < (thread_id+1)*NVALS/NTHREAD; i++) {
        auto v = intToBytes(i);
        std::string str(v.begin(),v.end());
        TRANSACTION_E {
            auto val = art.lookup(str);
            assert((int) val == i);
        } RETRY_E(true);
    }
}

void eraseKey(int thread_id) {
    TThread::set_id(thread_id);

    for (int i = thread_id*(NVALS/NTHREAD); i < (thread_id+1)*NVALS/NTHREAD; i++) {
        auto v = intToBytes(i);
        std::string str(v.begin(),v.end());
        TRANSACTION_E {
            art.erase(str);
        } RETRY_E(true);
    }
}

void words() {
    TART a;
    std::ifstream input("/usr/share/dict/words");
    int i = 0;
    for (std::string line; getline(input, line);) {
        printf("%s\n", line.c_str());
        TRANSACTION_E {
            a.insert(line, i);
        } RETRY_E(true);
        i++;
    }
    input.close();
    std::ifstream input2("/usr/share/dict/words");
    printf("lookup\n");
    i = 0;
    for (std::string line; getline(input2, line);) {
        TRANSACTION_E {
            assert(a.lookup(line) == i);
        } RETRY_E(true);
        i++;
    }
    printf("done\n");
    input2.close();
}

int main() {
    art = TART();
    printMem();

    // Build tree
    {
        auto starttime = std::chrono::system_clock::now();
        std::thread threads[NTHREAD];
        for (int i = 0; i < NTHREAD; i++) {
            threads[i] = std::thread(insertKey, i);
        }

        for (int i = 0; i < NTHREAD; i++) {
            threads[i].join();
        }
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now() - starttime);
        printf("insert,%d,%f\n\n", NVALS, (NVALS * 1.0) / duration.count());
    }
    printMem();

    {
        auto starttime = std::chrono::system_clock::now();
        std::thread threads[NTHREAD];
        for (int i = 0; i < NTHREAD; i++) {
            threads[i] = std::thread(lookupKey, i);
        }

        for (int i = 0; i < NTHREAD; i++) {
            threads[i].join();
        }
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now() - starttime);
        printf("lookup,%d,%f\n\n", NVALS, (NVALS * 1.0) / duration.count());
    }
    printMem();

    {
        auto starttime = std::chrono::system_clock::now();
        std::thread threads[NTHREAD];
        for (int i = 0; i < NTHREAD; i++) {
            threads[i] = std::thread(eraseKey, i);
        }

        for (int i = 0; i < NTHREAD; i++) {
            threads[i].join();
        }
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now() - starttime);
        printf("erase,%d,%f\n\n", NVALS, (NVALS * 1.0) / duration.count());
    }
    printMem();

    // words();
}
