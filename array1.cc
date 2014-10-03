#include <string>
#include <iostream>
#include <assert.h>
#include "Transaction.hh"
#include "Array1.hh"

void testSimpleInt() {
    Array1<int, 100> f;

    Transaction t;
    f.transWrite(t, 1, 100);
    assert(t.commit());

    Transaction t2;
    int f_read = f.transRead(t2, 1);
    
    assert(f_read == 100);
    assert(t2.commit());
    printf("PASS: testSimpleInt\n");
}

void testSimpleString() {
    Array1<std::string, 100> f;

    Transaction t;
    f.transWrite(t, 1, "100");
    assert(t.commit());

    Transaction t2;
    std::string f_read = f.transRead(t2, 1);
    
    assert(f_read.compare("100") == 0);
    assert(t2.commit());
    printf("PASS: testSimpleString\n");
}

int main() {
    testSimpleInt();
    testSimpleString();
    return 0;
}
