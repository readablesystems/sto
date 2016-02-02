#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include "Transaction.hh"
#include "List_snapshot.hh"

void testSimple() {
    List<int> l;
    Transaction t;
    Sto::set_transaction(&t);
    l.transInsert(1);
    l.transInsert(2);
    l.transInsert(3);
    assert(t.try_commit());

    uint64_t ss0 = Sto::take_snapshot();

    Transaction t1;
    Sto::set_transaction(&t1);
    l.transDelete(2);
    l.transInsert(5);
    assert(t1.try_commit());

    uint64_t ss1 = Sto::take_snapshot();

    Sto::set_active_sid(ss0);
    assert(l.find(1));
    assert(l.find(2));
    assert(l.find(3));
    assert(!l.find(5));

    Sto::set_active_sid(ss1);
    assert(l.find(1));
    assert(!l.find(2));
    assert(l.find(3));
    assert(l.find(5));

    std::cout << "PASS: simple test" << std::endl;
}

int main() {
    testSimple();
    return 0;
}
