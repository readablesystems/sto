#pragma once

#include "Sto.hh"
#include "VersionSelector.hh"
#include "YCSB_structs.hh"

namespace ver_sel {

template <typename VersImpl>
class VerSel<ycsb::ycsb_value, VersImpl> : public VerSelBase<VerSel<ycsb::ycsb_value, VersImpl>, VersImpl> {
public:
    typedef VersImpl version_type;
    static constexpr size_t num_versions = 2;

    explicit VerSel(type v) : vers_() {
        new (&vers_[0]) version_type(v);
    }
    
    VerSel(type v, bool insert) : vers_() {
        new (&vers_[0]) version_type(v, insert);
    }

    constexpr static int map_impl(int col_n) {
        typedef ycsb::ycsb_value::NamedColumn nc;
        auto col_name = static_cast<nc>(col_n);
        if (col_name == nc::odd_columns)
            return 0;
        else
            return 1;
    }

    version_type& version_at_impl(int cell) {
        return vers_[cell];
    }

    void install_by_cell_impl(ycsb::ycsb_value *dst, const ycsb::ycsb_value *src, int cell) {
        if (cell != 1 && cell != 0) {
            always_assert(false, "Invalid cell id.");
        }
        if (cell == 0) {
            dst->odd_columns = src->odd_columns;
        } else {
            dst->even_columns = src->even_columns;
        }
    }

private:
    version_type vers_[num_versions];
};

}
