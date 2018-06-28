#undef NDEBUG
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
#define KEYSIZE 5

TART art;
std::string keys[NVALS];
unsigned vals[NVALS];

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

int main() {
    art = TART();

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
        printf("insert,%d,%f\n", NVALS, (NVALS * 1.0) / duration.count());
    }

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
}
