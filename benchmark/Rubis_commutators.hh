#pragma once

#include "Commutators.hh"
#include "Rubis_structs.hh"

namespace commutators {

#if TPCC_SPLIT_TABLE
using item_row = rubis::item_comm_row;
#else
using item_row = rubis::item_row;
#endif

template <>
class Commutator<item_row> {
public:
    Commutator() = default;
    explicit Commutator(uint32_t bid) : max_bid(bid), date() {}
    explicit Commutator(uint32_t qty, uint32_t time) : max_bid(qty), date(time) {}

    item_row& operate(item_row& val) const {
        if (date != 0) {
            val.quantity -= max_bid;
            if (val.quantity == 0) {
                val.end_date = date;
            }
        } else {
            if (max_bid > val.max_bid) {
                val.max_bid = max_bid;
            }
            val.nb_of_bids += 1;
        }
        return val;
    }

private:
    uint32_t max_bid;
    uint32_t date;
};

}
