#undef NDEBUG
#include <string>
#include <iostream>
#include <cassert>
#include <vector>
#include "Sto.hh"
#include "TART.hh"
#include "Transaction.hh"
#include <unistd.h>


std::string absentkey1 = "absent";
std::string absentkey2 = "willbewritten";
TART aTART;

void CleanATART() {
    {
        TransactionGuard t;
        aTART.erase(absentkey2);
        aTART.erase(absentkey1);
    }
}

void ReadK1WriteK2() {
    {
        TransactionGuard t;
        auto x = aTART.lookup(absentkey1);
        aTART.insert(absentkey2, 123);
        usleep(1000);
    }
}

void WriteK2() {
    {
        // usleep(200);
        TransactionGuard t;
        aTART.insert(absentkey2, 456);
    }

}
void DelayWriteK2() {
    {
        TransactionGuard t;
        usleep(1000);
        auto x = aTART.lookup(absentkey2);
        printf("looked up absent key 2 is %d\n", x);
        aTART.insert(absentkey2, 123);
    }

}

void WriteK1K2() {
    {
        TransactionGuard t;
        aTART.insert(absentkey1, 101112);
        aTART.insert(absentkey1, 131415);
    }   
}

void ABA1() {
    try {
        TransactionGuard t;
        auto x = aTART.lookup(absentkey2);
        aTART.insert(absentkey1, 123);
        usleep(20000);
        printf("ABA1 TO COMMIT\n");
    } catch (Transaction::Abort e) {
        printf("aba1 aborted\n");
    }
}

void ABA2() {
    try {
        TransactionGuard t;
        usleep(1000);
        aTART.erase(absentkey2);
        printf("ABA2 TO COMMIT\n");
    } catch (Transaction::Abort e) {
        printf("aba 2 abored\n");
    }
}

void ABA3() {
    try {
        TransactionGuard t;
        usleep(10000);
        aTART.insert(absentkey2, 456);
        printf("ABA3 TO COMMIT\n");
    } catch (Transaction::Abort e) {
        printf("aba3 aborted\n");
    }
}