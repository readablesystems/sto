#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include "Transaction.hh"
#include "List1.hh"
#include "Array1.hh"

void testSimpleInt() {
	List1<int> f;

	Transaction t;
    Sto::set_transaction(&t);
	f.transInsert(100);
	assert(t.try_commit());

	Transaction t2;
    Sto::set_transaction(&t2);
	assert(f.transFind(100));

	assert(t2.try_commit());
	printf("PASS: testSimpleInt\n");
}

void testSimpleString() {
	List1<std::string> f;

	Transaction t;
    Sto::set_transaction(&t);
	f.transInsert("100");
	assert(t.try_commit());

	Transaction t2;
    Sto::set_transaction(&t2);
	assert(f.transFind("100"));

	assert(t2.try_commit());
	printf("PASS: testSimpleString\n");
}

void testIter() {
    std::vector<int> arr;
    List1<int> f;
    for (int i = 0; i < 10; i++) {
        int x = rand();
        arr.push_back(x);
        f.insert(x);
    }
    int max;
    TRANSACTION {
        max = *(std::max_element(f.begin(), f.end()));
    } RETRY(false);
    
    assert(max == *(std::max_element(arr.begin(), arr.end())));
    printf("Max is %i\n", max);
    printf("PASS: array max_element test\n");
    
}


void testConflictingIter() {
    List1<int> f;
    for (int i = 0; i < 10; i++) {
        f.insert(i);
    }

    Transaction t;
    Sto::set_transaction(&t);
    std::max_element(f.begin(), f.end());
    
    Transaction t1;
    Sto::set_transaction(&t1);
    f.transInsert(10);
    assert(t1.try_commit());
    assert(!t.try_commit());
    printf("PASS: conflicting array max_element test\n");
    
}

void testModifyingIter() {
    List1<int> f;
    for (int i = 0; i < 10; i++) {
        f.insert(i);
    }
    
    Transaction t;
    Sto::set_transaction(&t);
    std::replace(f.begin(), f.end(), 4, 6);
    assert(t.try_commit());
    
    
    Transaction t1;
    Sto::set_transaction(&t1);
    assert(!f.transFind(4));
    assert(t1.try_commit());
    
    printf("PASS: array replace test\n");
}

void testConflictingModifyIter1() {
    List1<int, true> f;
    for (int i = 0; i < 10; i++) {
        f.insert(i);
    }
    
    Transaction t;
    Sto::set_transaction(&t);
    std::replace(f.begin(), f.end(), 4, 6);
    
    Transaction t1;
    Sto::set_transaction(&t1);
    f.transInsert(4);
    assert(t1.try_commit());
    
    assert(!t.try_commit());
    
    Transaction t2;
    Sto::set_transaction(&t2);
    assert(f.transFind(4));
    assert(t2.try_commit());
    
    printf("PASS: array conflicting replace test1\n");
}

void testConflictingModifyIter2() {
    List1<int> f;
    for (int i = 0; i < 10; i++) {
        f.insert(i);
    }
    
    Transaction t;
    Sto::set_transaction(&t);
    std::replace(f.begin(), f.end(), 4, 6);
    assert(t.try_commit());
    
    Transaction t1;
    Sto::set_transaction(&t1);
    f.transInsert(4);
    assert(t1.try_commit());
    
    Transaction t2;
    Sto::set_transaction(&t2);
    assert(f.transFind(4));
    assert(t2.try_commit());
    
    printf("PASS: array conflicting replace test2\n");
}

void testConflictingModifyIter3() {
    List1<int> f;
    for (int i = 0; i < 10; i++) {
        f.insert(i);
    }
    
    Transaction t1;
    Sto::set_transaction(&t1);
    f.transFind(4);
    
    Transaction t;
    Sto::set_transaction(&t);
    std::replace(f.begin(), f.end(), 4, 6);
    assert(t.try_commit());
    
    assert(!t1.try_commit());
    
    Transaction t2;
    Sto::set_transaction(&t2);
    assert(!f.transFind(4));
    assert(t2.try_commit());
    
    printf("PASS: array conflicting replace test3\n");
}


int main() {
	testSimpleInt();
	testSimpleString();
    testIter();
    testConflictingIter();
    testModifyingIter();
    testConflictingModifyIter1();
    testConflictingModifyIter2();
    testConflictingModifyIter3();
	return 0;
}
