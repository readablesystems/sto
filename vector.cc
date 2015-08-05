#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include "Transaction.hh"
#include "Vector.hh"

void testSimpleInt() {
	Vector<int> f;

	Transaction t;
    Sto::set_transaction(&t);
	f.push_back(100);
	assert(t.try_commit());

    Transaction t2;
    Sto::set_transaction(&t2);
	int f_read = f.transRead(0);

	assert(f_read == 100);
	assert(t2.try_commit());
	printf("PASS: testSimpleInt\n");
}

void testWriteNPushBack() {
    Vector<int> f;
    
    Transaction t;
    Sto::set_transaction(&t);
    f.push_back(10);
    assert(t.try_commit());
    
    Transaction t2;
    Sto::set_transaction(&t2);
    f.transWrite(0, 4);
    
    Transaction t3;
    Sto::set_transaction(&t3);
    f.push_back(20); // This will resize the array
    assert(t3.try_commit());
    
    assert(t2.try_commit()); // This should not conflict with the write
    
    Transaction t4;
    Sto::set_transaction(&t4);
    assert(f.transRead(0) == 4); // Make sure that the value is actually updated
    assert(t4.try_commit());
    
    printf("PASS: testWriteNPushBack\n");
}

void testPushBack() {
    Vector<int> f;
    
    Transaction t;
    Sto::set_transaction(&t);
    f.push_back(10);
    assert(t.try_commit());
    
    Transaction t2;
    Sto::set_transaction(&t2);
    f.push_back(4);
    
    Transaction t3;
    Sto::set_transaction(&t3);
    f.push_back(20);
    assert(t3.try_commit());
    
    assert(t2.try_commit()); // This should not conflict with the push_back
    
    Transaction t4;
    Sto::set_transaction(&t4);
    assert(f.transRead(0) == 10);
    assert(f.transRead(1) == 20);
    assert(f.transRead(2) == 4);
    assert(t4.try_commit());
    
    printf("PASS: testPushBack\n");
}


void testPushBackNRead() {
    Vector<int> f;
    
    Transaction t;
    Sto::set_transaction(&t);
    f.push_back(10);
    assert(t.try_commit());
    
    Transaction t2;
    Sto::set_transaction(&t2);
    f.push_back(4);
    assert(f.transRead(1) == 4);
    
    assert(t2.try_commit());
    printf("PASS: testPushBackNRead\n");
}

void testPushBackNRead1() {
    Vector<int> f;
    
    Transaction t;
    Sto::set_transaction(&t);
    f.push_back(10);
    assert(t.try_commit());
    
    Transaction t2;
    Sto::set_transaction(&t2);
    f.push_back(4);
    assert(f.transRead(1) == 4);
    
    Transaction t3;
    Sto::set_transaction(&t3);
    f.push_back(20);
    assert(t3.try_commit());
    
    assert(!t2.try_commit());
    printf("PASS: testPushBackNRead1\n");
}

void testPushBackNRead2() {
    Vector<int> f;
    
    Transaction t;
    Sto::set_transaction(&t);
    f.push_back(10);
    assert(t.try_commit());
    
    Transaction t2;
    Sto::set_transaction(&t2);
    f.push_back(4);
    assert(f.transRead(f.transSize() - 1) == 4);
    
    Transaction t3;
    Sto::set_transaction(&t3);
    f.push_back(20);
    assert(t3.try_commit());
    
    assert(!t2.try_commit()); // TODO: this can actually commit
    printf("PASS: testPushBackNRead2\n");
}


void testPushBackNWrite() {
    Vector<int> f;
    
    Transaction t;
    Sto::set_transaction(&t);
    f.push_back(10);
    assert(t.try_commit());
    
    Transaction t2;
    Sto::set_transaction(&t2);
    f.push_back(4);
    f.transWrite(1, 10);
    
    assert(t2.try_commit());
    
    Transaction t3;
    Sto::set_transaction(&t3);
    assert(f.transRead(1) == 10);
    assert(t3.try_commit());
    printf("PASS: testPushBackNRead\n");
}



void testSimpleString() {
	Vector<std::string> f;

    Transaction t;
    Sto::set_transaction(&t);
	f.push_back("100");
	assert(t.try_commit());

	Transaction t2;
    Sto::set_transaction(&t2);
	std::string f_read = f.transRead(0);

	assert(f_read.compare("100") == 0);
	assert(t2.try_commit());
	printf("PASS: testSimpleString\n");
}

void testIter() {
    std::vector<int> arr;
    Vector<int> f;
    TRANSACTION {
    for (int i = 0; i < 10; i++) {
        int x = rand();
        arr.push_back(x);
        f.push_back(x);
    }
    } RETRY(false)
    
    int max;
    TRANSACTION {
        max = *(std::max_element(f.begin(), f.end()));
    } RETRY(false)
    
    assert(max == *(std::max_element(arr.begin(), arr.end())));
    printf("Max is %i\n", max);
    printf("PASS: vector max_element test\n");
    
}


void testConflictingIter() {
    Vector<int> f;
    TRANSACTION {
    for (int i = 0; i < 10; i++) {
        f.push_back(i);
    }
    } RETRY(false)

    Transaction t;
    Sto::set_transaction(&t);
    std::max_element(f.begin(), f.end());
    
    Transaction t1;
    Sto::set_transaction(&t1);
    f.transWrite(4, 10);
    assert(t1.try_commit());
    assert(!t.try_commit());
    printf("PASS: conflicting vector max_element test\n");
    
}

void testModifyingIter() {
    Vector<int> f;
    TRANSACTION {
    for (int i = 0; i < 10; i++) {
        f.push_back(i);
    }
    } RETRY(false)
    
    Transaction t;
    Sto::set_transaction(&t);
    std::replace(f.begin(), f.end(), 4, 6);
    assert(t.try_commit());
    
    
    Transaction t1;
    Sto::set_transaction(&t1);
    int v = f.transRead(4);
    assert(t1.try_commit());
    
    assert(v == 6);
    printf("PASS: vector replace test\n");
}

void testConflictingModifyIter1() {
    Vector<int> f;
    TRANSACTION {
    for (int i = 0; i < 10; i++) {
        f.push_back(i);
    }
    } RETRY(false)
    
    Transaction t;
    Sto::set_transaction(&t);
    std::replace(f.begin(), f.end(), 4, 6);
    
    Transaction t1;
    Sto::set_transaction(&t1);
    f.transWrite(4, 10);
    assert(t1.try_commit());
    
    assert(!t.try_commit());
    
    Transaction t2;
    Sto::set_transaction(&t2);
    int v = f.transRead(4);
    assert(t2.try_commit());
    
    assert(v == 10);
    
    printf("PASS: vector conflicting replace test1\n");
}

void testConflictingModifyIter2() {
    Vector<int> f;
    TRANSACTION {
    for (int i = 0; i < 10; i++) {
        f.push_back(i);
    }
    } RETRY(false);
    
    Transaction t;
    Sto::set_transaction(&t);
    std::replace(f.begin(), f.end(), 4, 6);
    assert(t.try_commit());
    
    Transaction t1;
    Sto::set_transaction(&t1);
    f.transWrite(4, 10);
    assert(t1.try_commit());
    
    Transaction t2;
    Sto::set_transaction(&t2);
    int v = f.transRead(4);
    assert(t2.try_commit());
    
    assert(v == 10);
    
    printf("PASS: vector conflicting replace test2\n");
}

void testConflictingModifyIter3() {
    Vector<int> f;
    TRANSACTION {
    for (int i = 0; i < 10; i++) {
        f.push_back(i);
    }
    } RETRY(false)
    
    Transaction t1;
    Sto::set_transaction(&t1);
    f.transRead(4);
    
    Transaction t;
    Sto::set_transaction(&t);
    std::replace(f.begin(), f.end(), 4, 6);
    assert(t.try_commit());
    
    assert(!t1.try_commit());
    
    Transaction t2;
    Sto::set_transaction(&t2);
    int v = f.transRead(4);
    assert(t2.try_commit());
    
    assert(v == 6);
    
    printf("PASS: vector conflicting replace test3\n");
}

void testIterNPushBack() {
    Vector<int> f;
    TRANSACTION {
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    } RETRY(false)
    
    int max;
    TRANSACTION {
        f.push_back(20);
        max = *(std::max_element(f.begin(), f.end()));
    } RETRY(false)
    
    assert(max == 20);
    printf("PASS: IterNPushBack\n");
    
}

void testIterNPushBack1() {
    Vector<int> f;
    TRANSACTION {
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    } RETRY(false)
    
    int max;
    Transaction t1;
    Sto::set_transaction(&t1);
    f.push_back(20);
    max = *(std::max_element(f.begin(), f.end()));
    assert(max == 20);
    
    Transaction t2;
    Sto::set_transaction(&t2);
    f.push_back(12);
    assert(t2.try_commit());
    
    assert(!t1.try_commit());
    
    printf("PASS: IterNPushBack1\n");    
}

void testErase() {
    Vector<int> f;
    
    TRANSACTION {
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    } RETRY(false)
    
    Transaction t1;
    Sto::set_transaction(&t1);
    f.erase(f.begin() + 5);
    assert(t1.try_commit());
    
    TRANSACTION {
        Vector<int>::iterator it = f.begin();
        Vector<int>::iterator eit = f.end();
        for (; it != eit; ++it) {
            printf("%i\n", (int)*it);
        }
    } RETRY(false)
    
    printf("PASS: testErase\n");
}

void testInsert() {
    Vector<int> f;
    
    TRANSACTION {
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    } RETRY(false)
    
    Transaction t1;
    Sto::set_transaction(&t1);
    f.insert(f.begin() + 5, 25);
    assert(t1.try_commit());
    
    TRANSACTION {
        Vector<int>::iterator it = f.begin();
        Vector<int>::iterator eit = f.end();
        for (; it != eit; ++it) {
            printf("%i\n", (int)*it);
        }
    } RETRY(false)
    
    printf("PASS: testInsert\n");
}





int main() {
	testSimpleInt();
    testWriteNPushBack();
    testPushBack();
    testPushBackNRead();
    testPushBackNRead1();
    testPushBackNRead2();
    testPushBackNWrite();
	testSimpleString();
    testIter();
    testConflictingIter();
    testModifyingIter();
    testConflictingModifyIter1();
    testConflictingModifyIter2();
    testConflictingModifyIter3();
    testIterNPushBack();
    testIterNPushBack1();
    testErase();
    testInsert();
	return 0;
}
