#include <bitset>

#include "MVCCStructs.hh"

std::ostream& operator<<(std::ostream& w, MvStatus s) {
    return w << mvstatus::stringof(s);
}

namespace mvstatus {
std::string stringof(MvStatus s) {
    bool empty = true;
    std::ostringstream status;
    std::array statuses  {UNUSED, DELETED, PENDING, COMMITTED, DELTA, LOCKED, GARBAGE, GARBAGE2, POISONED, RESPLIT, ENQUEUED};
    std::array statusstr {"UNUSED", "DELETED", "PENDING", "COMMITTED", "DELTA", "LOCKED", "GARBAGE", "GARBAGE2", "POISONED", "RESPLIT", "ENQUEUED"};
    for (size_t i = 0; i < statuses.size(); ++i) {
        if (s & statuses[i]) {
            if (!empty) {
                status << ' ';
            }
            status << statusstr[i];
            empty = false;
            s = static_cast<MvStatus>(static_cast<unsigned>(s) & ~statuses[i]);
        }
    }

    if (s & ABORTED) {
        if (!empty) {
            status << ' ';
        }
        switch (s) {
            case ABORTED:                status << "ABORTED"; break;
            case ABORTED_RV:             status << "ABORTED_RV"; break;
            case ABORTED_WV1:            status << "ABORTED_WV1"; break;
            case ABORTED_WV2:            status << "ABORTED_WV2"; break;
            case ABORTED_TXNV:           status << "ABORTED_TXNV"; break;
            case ABORTED_POISON:         status << "ABORTED_POISON"; break;
            default:
                std::ostringstream status;
                status << "[" << (unsigned long)s << "]";
                break;
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
