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
    f.push_back(200);
    *(f.begin()) = *(f.begin() + 1);
	assert(t.try_commit());

    Transaction t2;
    Sto::set_transaction(&t2);
	int f_read = f.transGet(0);

	assert(f_read == 200);
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
    f.transUpdate(0, 4);
    
    Transaction t3;
    Sto::set_transaction(&t3);
    f.push_back(20); // This will resize the array
    assert(t3.try_commit());
    
    Sto::set_transaction(&t2);
    assert(t2.try_commit()); // This should not conflict with the write
    
    Transaction t4;
    Sto::set_transaction(&t4);
    assert(f.transGet(0) == 4); // Make sure that the value is actually updated
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
    assert(f.transGet(0) == 10);
    assert(f.transGet(1) == 20);
    assert(f.transGet(2) == 4);
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
    assert(f.transGet(1) == 4);
    
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
    assert(f.transGet(1) == 4);
    
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
    assert(f.transGet(f.transSize() - 1) == 4);
    
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
    f.transUpdate(1, 10);
    
    assert(t2.try_commit());
    
    Transaction t3;
    Sto::set_transaction(&t3);
    assert(f.transGet(1) == 10);
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
	std::string f_read = f.transGet(0);

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
        f.push_back(x);
        arr.push_back(x);
    }
    } RETRY(false);

    int max;
    TRANSACTION {
        max = *(std::max_element(f.begin(), f.end()));
    } RETRY(false);
    
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
    } RETRY(false);

    Transaction t;
    Sto::set_transaction(&t);
    std::max_element(f.begin(), f.end());
    
    Transaction t1;
    Sto::set_transaction(&t1);
    f.transUpdate(4, 10);
    assert(t1.try_commit());
    assert(!t.try_commit());
    printf("PASS: conflicting vector max_element test\n");
    
}

void testModifyingIter() {
    Vector<int> f;
    Transaction tt;
    Sto::set_transaction(&tt);
    for (int i = 0; i < 10; i++) {
        f.push_back(i);
    }
    assert(tt.try_commit());
    
    Transaction t;
    Sto::set_transaction(&t);
    std::replace(f.begin(), f.end(), 4, 6);
    assert(t.try_commit());
    
    
    Transaction t1;
    Sto::set_transaction(&t1);
    int v = f.transGet(4);
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
    } RETRY(false);
    
    Transaction t;
    Sto::set_transaction(&t);
    std::replace(f.begin(), f.end(), 4, 6);
    
    Transaction t1;
    Sto::set_transaction(&t1);
    f.transUpdate(4, 10);
    assert(t1.try_commit());
    
    assert(!t.try_commit());
    
    Transaction t2;
    Sto::set_transaction(&t2);
    int v = f.transGet(4);
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
    f.transUpdate(4, 10);
    assert(t1.try_commit());
    
    Transaction t2;
    Sto::set_transaction(&t2);
    int v = f.transGet(4);
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
    } RETRY(false);
    
    Transaction t1;
    Sto::set_transaction(&t1);
    f.transGet(4);
    
    Transaction t;
    Sto::set_transaction(&t);
    std::replace(f.begin(), f.end(), 4, 6);
    assert(t.try_commit());
    
    assert(!t1.try_commit());
    
    Transaction t2;
    Sto::set_transaction(&t2);
    int v = f.transGet(4);
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
    } RETRY(false);
    
    int max;
    TRANSACTION {
        f.push_back(20);
        max = *(std::max_element(f.begin(), f.end()));
    } RETRY(false);
    
    assert(max == 20);
    printf("PASS: IterNPushBack\n");
    
}

void testIterNPushBack1() {
    Vector<int> f;
    Transaction tt;
    Sto::set_transaction(&tt);
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    assert(tt.try_commit());
    
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

void testIterNPushBack2() {
    Vector<int> f;
    Transaction tt;
    Sto::set_transaction(&tt);

        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    assert(tt.try_commit());
    
    Transaction t1;
    Sto::set_transaction(&t1);
    std::max_element(f.begin(), f.end());
    
    Transaction t2;
    Sto::set_transaction(&t2);
    f.push_back(2);
    assert(t2.try_commit());
    
    assert(!t1.try_commit());
    
    printf("PASS: IterNPushBack2\n");
    
}


void testErase() {
    Vector<int> f;
    
    TRANSACTION {
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    } RETRY(false);
    
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
    } RETRY(false);
    
    printf("PASS: testErase\n");
}

void testInsert() {
    Vector<int> f;
    
    TRANSACTION {
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    } RETRY(false);
    
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
    } RETRY(false);
    
    printf("PASS: testInsert\n");
}


void testPushNPop() {
    Vector<int> f;
    
    TRANSACTION {
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    } RETRY(false);
    
    TRANSACTION {
        f.push_back(20);
        f.push_back(21);
        assert(f.transGet(0) == 0);
        assert(f.transSize() == 12);
        f.pop_back();
        f.pop_back();
        
    } RETRY(false);
    
    Transaction t1;
    Sto::set_transaction(&t1);
    f.pop_back();
    f.push_back(15);
    
    Transaction t2;
    Sto::set_transaction(&t2);
    f.push_back(20);
    assert(t2.try_commit());
    
    assert(!t1.try_commit());
    
    TRANSACTION {
        assert(f.transSize() == 11);
        assert(f.transGet(10) == 20);
    } RETRY(false);
    
    
    printf("PASS: testPushNPop\n");
    
    Transaction t3;
    Sto::set_transaction(&t3);
    f.pop_back();
    f.push_back(15);
    
    Transaction t4;
    Sto::set_transaction(&t4);
    f.pop_back();
    assert(t4.try_commit());
    
    assert(!t3.try_commit());
    
    TRANSACTION {
        assert(f.transSize() == 10);
    } RETRY(false);
    
    printf("PASS: testPushNPop1\n");
    

    Transaction t5;
    Sto::set_transaction(&t5);
    f.pop_back();
    f.pop_back();
    f.push_back(15);
    
    Transaction t6;
    Sto::set_transaction(&t6);
    f.transUpdate(8, 16);
    assert(t6.try_commit());
    
    Transaction t7;
    Sto::set_transaction(&t7);
    f.transGet(8);
    
    assert(t5.try_commit());
    assert(!t7.try_commit());
    
    TRANSACTION {
        assert(f.transSize() == 9);
        assert(f.transGet(8) == 15);
    } RETRY(false);
    
    printf("PASS: testPushNPop2\n");

}

void testPopAndUdpate() {
    Vector<int> f;
    
    TRANSACTION {
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    } RETRY(false);
    
    Transaction t1;
    Sto::set_transaction(&t1);
    f.transUpdate(9, 20);
    
    
    Transaction t2;
    Sto::set_transaction(&t2);
    f.pop_back();
    f.pop_back();
    assert(t2.try_commit());
    Sto::set_transaction(&t1);
    try {
        assert(!t1.try_commit());
    } catch (Transaction::Abort) {}
}

void testMulPushPops() {
    Vector<int> f;
    
    TRANSACTION {
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    } RETRY(false);
    
    Transaction t1;
    Sto::set_transaction(&t1);
    f.push_back(100);
    f.push_back(200);
    f.pop_back();
    f.pop_back();
    f.pop_back();
    assert(t1.try_commit());
}

void testMulPushPops1() {
    Vector<int> f;
    
    TRANSACTION {
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    } RETRY(false);
    
    Transaction t1;
    Sto::set_transaction(&t1);
    f.push_back(100);
    f.push_back(200);
    f.pop_back();
    f.pop_back();
    f.push_back(300);
    assert(t1.try_commit());
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
    testIterNPushBack2();
    testErase();
    testInsert();
    testPushNPop();
    testPopAndUdpate();
    testMulPushPops();
    testMulPushPops1();
	return 0;
}
