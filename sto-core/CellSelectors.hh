// Base interface for selecting cells from a row

#pragma once

#include "MVCC_structs.hh"

namespace cell_sel {

template <typename T>
// Default cell selector: always select cell 0
class CellSelector<MvObject<T>> {
public:
    typedef MvObject<T> object_type;
    typedef typename object_type::history_type history_type;

    CellSelector() = default;

    constexpr static int map(int) {
        return 0;
    }

    T* read(bench::RowAccess access, TransProxy item, const bool has_insert) {
        history_type *h = std::get<0>(cells).find(Sto::read_tid());
        if (h->status_is(DELETED) && !has_insert) {
            return nullptr;
        }
        if (access != RowAccess::None) {
            MvAccess::template read<value_type>(item.item(), h);
            auto vp = h->vp();
            assert(vp);
            return vp;
        } else {
            auto vp = &(std::get<0>(cells).nontrans_access());
            assert(vp);
            return vp;
        }
    }

private:
    std::tuple<MvObject<T>> cells();
};

};
