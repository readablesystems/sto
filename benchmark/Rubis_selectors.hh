#pragma once

#include "Sto.hh"
#include "VersionSelector.hh"
#include "Rubis_structs.hh"

namespace ver_sel {

template<typename VersImpl>
class VerSel<rubis::item_row, VersImpl> : public VerSelBase<VerSel<rubis::item_row, VersImpl>, VersImpl> {
public:
    typedef VersImpl version_type;
    static constexpr size_t num_versions = 2;

    explicit VerSel(type v) : vers_() {
        new(&vers_[0]) version_type(v);
    }

    VerSel(type v, bool insert) : vers_() {
        new(&vers_[0]) version_type(v, insert);
    }

    constexpr static int map_impl(int col_n) {
        typedef rubis::item_row::NamedColumn nc;
        auto col_name = static_cast<nc>(col_n);
        if (col_name == nc::quantity || col_name == nc::max_bid
            || col_name == nc::nb_of_bids || col_name == nc::end_date) {
            return 0;
        } else
            return 1;
    }

    version_type &version_at_impl(int cell) {
        return vers_[cell];
    }

    void install_by_cell_impl(rubis::item_row *dst, const rubis::item_row *src, int cell) {
        switch (cell) {
            case 0:
                dst->quantity = src->quantity;
                dst->nb_of_bids = src->nb_of_bids;
                dst->max_bid = src->max_bid;
                dst->end_date = src->end_date;
                break;
            case 1:
                dst->name = src->name;
                dst->description = src->description;
                dst->initial_price = src->initial_price;
                dst->reserve_price = src->reserve_price;
                dst->buy_now = src->buy_now;
                dst->start_date = src->start_date;
                dst->seller = src->seller;
                dst->category = src->category;
                break;
            default:
                always_assert(false, "cell id out of bound\n");
                break;
        }
    }

private:
    version_type vers_[num_versions];
};

}
