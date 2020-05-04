#pragma once

#include "Sto.hh"
#include "VersionSelector.hh"
#include "HT_structs.hh"

namespace ver_sel {

template <typename VersImpl>
class VerSel<htbench::ht_value, VersImpl> : public VerSelBase<VerSel<htbench::ht_value, VersImpl>, VersImpl> {
public:
    typedef VersImpl version_type;
    static constexpr size_t num_versions = 1;

    explicit VerSel(type v) : vers_() {
        new (&vers_[0]) version_type(v);
    }

    VerSel(type v, bool insert) : vers_() {
        new (&vers_[0]) version_type(v, insert);
    }

    constexpr static int map_impl(int col_n) {
        return 0;
    }

    version_type& version_at_impl(int cell) {
        return vers_[cell];
    }

    void install_by_cell_impl(htbench::ht_value *dst, const htbench::ht_value *src, int) {
        *dst = *src;
    }

private:
    version_type vers_[num_versions];
};

}
