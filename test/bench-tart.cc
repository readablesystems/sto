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
#include <random>

int nthread = 1;
int rand_keys = 0;

#define NVALS 10000000

uint64_t* keys;

TART* art;

std::vector<unsigned char> intToBytes(int paramInt)
{
    std::vector<unsigned char> arrayOfByte(4);
     for (int i = 0; i < 4; i++)
         arrayOfByte[3 - i] = (paramInt >> (i * 8));
     return arrayOfByte;
}

void insertKey(int thread_id) {
    TThread::set_id(thread_id);

    for (int i = thread_id*(NVALS/nthread); i < (thread_id+1)*NVALS/nthread; i++) {
        // auto v = intToBytes(keys[i]);
        // std::string str(v.begin(),v.end());
        TRANSACTION_E {
            art->insert(keys[i], keys[i]);
        } RETRY_E(true);
    }
}

void lookupKey(int thread_id) {
    TThread::set_id(thread_id);

    for (int i = thread_id*(NVALS/nthread); i < (thread_id+1)*NVALS/nthread; i++) {
        // auto v = intToBytes(keys[i]);
        // std::string str(v.begin(),v.end());
        TRANSACTION_E {
            auto val = art->lookup(keys[i]);
            assert(val == keys[i]);
        } RETRY_E(true);
    }
}

void eraseKey(int thread_id) {
    TThread::set_id(thread_id);

    for (int i = thread_id*(NVALS/nthread); i < (thread_id+1)*NVALS/nthread; i++) {
        // auto v = intToBytes(keys[i]);
        // std::string str(v.begin(),v.end());
        TRANSACTION_E {
            art->erase(keys[i]);
        } RETRY_E(true);
    }
}

// void words() {
//     TART a;
//     std::ifstream input("/usr/share/dict/words");
//     int i = 0;
//     for (std::string line; getline(input, line);) {
//         printf("%s\n", line.c_str());
//         TRANSACTION_E {
//             a.insert(line, i);
//         } RETRY_E(true);
//         i++;
//     }
//     input.close();
//     std::ifstream input2("/usr/share/dict/words");
//     printf("lookup\n");
//     i = 0;
//     for (std::string line; getline(input2, line);) {
//         TRANSACTION_E {
//             assert(a.lookup(line) == i);
//         } RETRY_E(true);
//         i++;
//     }
//     printf("done\n");
//     input2.close();
// }

int main(int argc, char *argv[]) {
    if (argc > 1) {
        nthread = atoi(argv[1]);
    }
    if (argc > 2) {
        rand_keys = atoi(argv[2]);
    }

    pthread_t advancer;
    pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
    pthread_detach(advancer); 

    art = new TART();

    keys = new uint64_t[NVALS];

    std::mt19937 rng;
    rng.seed(std::random_device()());
    std::uniform_int_distribution<std::mt19937::result_type> dist(0,(unsigned) -1);

    for (uint64_t i = 0; i < NVALS; i++) {
        if (rand_keys) {
            keys[i] = dist(rng);
        } else {
            keys[i] = i;
        }
    }

    // Build tree
    {
        auto starttime = std::chrono::system_clock::now();
        std::thread threads[nthread];
        for (int i = 0; i < nthread; i++) {
            threads[i] = std::thread(insertKey, i);
        }

        for (int i = 0; i < nthread; i++) {
            threads[i].join();
        }
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now() - starttime);
        printf("insert,%d,%f\n", NVALS, (NVALS * 1.0) / duration.count());
    }

    {
        auto starttime = std::chrono::system_clock::now();
        std::thread threads[nthread];
        for (int i = 0; i < nthread; i++) {
            threads[i] = std::thread(lookupKey, i);
        }

        for (int i = 0; i < nthread; i++) {
            threads[i].join();
        }
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now() - starttime);
        printf("lookup,%d,%f\n", NVALS, (NVALS * 1.0) / duration.count());
    }

    {
        auto starttime = std::chrono::system_clock::now();
        std::thread threads[nthread];
        for (int i = 0; i < nthread; i++) {
            threads[i] = std::thread(eraseKey, i);
        }

        for (int i = 0; i < nthread; i++) {
            threads[i].join();
        }
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now() - starttime);
        printf("erase,%d,%f\n", NVALS, (NVALS * 1.0) / duration.count());
    }
    delete art;

    // words();
}
