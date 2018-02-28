#undef NDEBUG
#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include "Transaction.hh"
#include "SwissTArray.hh"
#include "TBox.hh"

void testSimpleInt() {
	SwissTArray<int, 100> f;

    {
        TransactionGuard t;
        f[1] = 100;
        f[1] = 200;
    }

	{
        TransactionGuard t2;
        int f_read = f[1];
        assert(f_read == 200);
    }

	printf("PASS: %s\n", __FUNCTION__);
}

void testWriteWriteConflict() {
	SwissTArray<int, 100> f;

	{
		TestTransaction t1(1);

		TestTransaction t2(2);

		try {
			t1.use();
			f[1] = 100;

			t2.use();
			f[1] = 100;
		} catch (Transaction::Abort e) {		
		}	

		assert(t2.get_tx().is_restarted());

		t1.use();
		assert(t1.try_commit());

		t2.use();
		Sto::start_transaction();
		int a = f[1];
		assert(a == 100);	

		f[1] = 200;
		a = f[1];
		assert(a == 200);

	}

	printf("PASS: %s\n", __FUNCTION__);
}

void testAbortReleaseLock() {
	std::cout << "Starting test abort release lock!" << std::endl;
	SwissTArray<int, 100> f;

	{
		TestTransaction t1(1);

		TestTransaction t2(2);

		try {
			t1.use();
			f[1] = 100;

			t2.use();
			f[2] = 200;
			f[1] = 300;
		} catch(Transaction::Abort e) {

		}

		assert(t2.get_tx().is_restarted());

		t1.use();
		f[2] = 400;
		f[1] = 500;
		assert(t1.try_commit());

	}
	printf("PASS: %s\n", __FUNCTION__);
}

int main() {
    //testSimpleInt();
    testWriteWriteConflict();
    testAbortReleaseLock();
    std::cout << "Tests finished." << std::endl;
    return 0;
}
