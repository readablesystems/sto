#pragma once

#include "Commutators.hh"
#include "Rubis_structs.hh"

namespace commutators {

using item_row = rubis::item_row;
using item_row_infreq = rubis::item_row_infreq;
using item_row_frequpd = rubis::item_row_frequpd;

template <>
class Commutator<item_row> {
public:
    Commutator() = default;
    explicit Commutator(uint32_t bid) : max_bid(bid), date() {}
    explicit Commutator(uint32_t qty, uint32_t time) : max_bid(qty), date(time) {}

    void operate(item_row& val) const {
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
    }

    friend Commutator<item_row_infreq>;
    friend Commutator<item_row_frequpd>;

private:
    uint32_t max_bid;
    uint32_t date;
};

template <>
class Commutator<item_row_infreq> : public Commutator<item_row> {
public:
    Commutator() = default;

    template <typename... Args>
    Commutator(Args&&... args) : Commutator<item_row>(std::forward<Args>(args)...) {}

    void operate(item_row_infreq&) const {
    }
};

template <>
class Commutator<item_row_frequpd> : public Commutator<item_row> {
public:
    Commutator() = default;

    template <typename... Args>
    Commutator(Args&&... args) : Commutator<item_row>(std::forward<Args>(args)...) {}

    void operate(item_row_frequpd& val) const {
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
    }
};

}
