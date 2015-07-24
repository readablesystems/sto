#include <string>
#include <iostream>
#include <assert.h>
#include "Transaction.hh"
#include "Array1.hh"

void testSimpleInt() {
	Array1<int, 100> f;
  bool committed;
	TRANSACTION {
    f.transWrite(1, 100);
  } TRY_COMMIT(committed)
  assert(committed);

	TRANSACTION {
    int f_read = f.transRead(1);

    assert(f_read == 100);
	} TRY_COMMIT(committed)
  assert(committed);
	printf("PASS: testSimpleInt\n");
}

void testSimpleString() {
	Array1<std::string, 100> f;

	bool committed;
	TRANSACTION {
    f.transWrite(1, "100");
	} TRY_COMMIT(committed)
  assert(committed);

	TRANSACTION {
    std::string f_read = f.transRead(1);

    assert(f_read.compare("100") == 0);
	} TRY_COMMIT(committed)
  assert(committed);
	printf("PASS: testSimpleString\n");
}

int main() {
	testSimpleInt();
	testSimpleString();
	return 0;
}
