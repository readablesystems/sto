#include "MVCCStructs.hh"

std::ostream& operator<<(std::ostream& w, MvStatus s) {
    switch (s) {
    case UNUSED:                 return w << "UNUSED";
    case ABORTED:                return w << "ABORTED";
    case PENDING_DELETED:        return w << "PENDING_DELETED";
    case COMMITTED_DELETED:      return w << "COMMITTED_DELETED";
    case PENDING_DELTA:          return w << "PENDING_DELTA";
    case COMMITTED_DELTA:        return w << "COMMITTED_DELTA";
    case LOCKED_COMMITTED_DELTA: return w << "LOCKED_COMMITTED_DELTA";
    case PENDING:                return w << "PENDING";
    case COMMITTED:              return w << "COMMITTED";
    default:                     return w << "MvStatus[" << (int) s << "]";
    }
}

void MvHistoryBase::print_prevs(size_t max) const {
    auto h = this;
    for (size_t i = 0;
         h && i < max;
         ++i, h = h->prev_.load(std::memory_order_relaxed)) {
        std::cerr << i << ". " << (void*) h << " " << h->status_.load(std::memory_order_relaxed) << " W" << h->wtid_ << " R" << h->rtid_.load(std::memory_order_relaxed) << "\n";
    }
}
