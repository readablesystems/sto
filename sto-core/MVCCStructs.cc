#include <bitset>

#include "MVCCStructs.hh"

std::ostream& operator<<(std::ostream& w, MvStatus s) {
    return w << mvstatus::stringof(s);
}

namespace mvstatus {
std::string stringof(MvStatus s) {
    if (s & ABORTED) {
        switch (s) {
            case ABORTED:                return "ABORTED";
            case ABORTED_RV:             return "ABORTED_RV";
            case ABORTED_WV1:            return "ABORTED_WV1";
            case ABORTED_WV2:            return "ABORTED_WV2";
            case ABORTED_TXNV:           return "ABORTED_TXNV";
            case ABORTED_POISON:         return "ABORTED_POISON";
            default:
                std::ostringstream status;
                status << "MvStatus[" << (unsigned long)s << "]";
                return status.str();
        }
    }

    bool empty = true;
    std::ostringstream status;
    std::array statuses  {UNUSED, DELETED, PENDING, COMMITTED, DELTA, LOCKED, GARBAGE, GARBAGE2, POISONED, RESPLIT};
    std::array statusstr {"UNUSED", "DELETED", "PENDING", "COMMITTED", "DELTA", "LOCKED", "GARBAGE", "GARBAGE2", "POISONED", "RESPLIT"};
    for (size_t i = 0; i < statuses.size(); ++i) {
        if (s & statuses[i]) {
            if (!empty) {
                status << ' ';
            }
            status << statusstr[i];
            empty = false;
        }
    }

    return status.str();
}
}  // namespace mvstatus

/*
void MvHistoryBase::print_prevs(size_t max, size_t cell) const {
    auto h = this;
    for (size_t i = 0;
         h && i < max;
         ++i, h = h->prev_[cell].load(std::memory_order_relaxed)) {
        std::cerr << i << ". " << (void*) h << " ";
        uintptr_t oaddr = reinterpret_cast<uintptr_t>(h->obj_);
        if (reinterpret_cast<uintptr_t>(h) == oaddr + 24) {
            std::cerr << "INLINE ";
        }
        std::cerr << h->status_.load(std::memory_order_relaxed) << " W" << h->wtid_ << " R" << h->rtid_.load(std::memory_order_relaxed) << "\n";
    }
}

#if !NDEBUG
void MvHistoryBase::assert_status_fail(const char* description) {
    std::cerr << "MvHistoryBase::assert_status_fail: " << description << "\n";
    print_prevs(5);
    assert(false);
}
#endif
*/
