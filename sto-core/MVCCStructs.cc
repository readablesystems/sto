#include "MVCCStructs.hh"
#include "Transaction.hh"

void MvHistoryBase::update_btid() {
    btid(std::max(Transaction::_RTID.load(), btid()));
}
