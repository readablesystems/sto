#include "ITest.h"

__thread__	ITest::SlotInfo* ITest::_tls_slot_info = null;

int ITest::_num_post_read_write = 0;
