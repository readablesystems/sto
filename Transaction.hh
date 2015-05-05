#pragma once

#include "local_vector.hh"
#include "compiler.hh"
#include <algorithm>
#include <functional>
#include <memory>
#include <type_traits>
#include <unistd.h>
#include <iostream>
#define PERF_LOGGING 0
#define DETAILED_LOGGING 0
#define ASSERT_TX_SIZE 0
#define TRANSACTION_HASHTABLE 1

#if ASSERT_TX_SIZE
#if DETAILED_LOGGING
#  define TX_SIZE_LIMIT 20000
#  include <cassert>
#else
#  error "ASSERT_TX_SIZE requires DETAILED_LOGGING!"
#endif
#endif

#if DETAILED_LOGGING
#if PERF_LOGGING
#else
#  error "DETAILED_LOGGING requires PERF_LOGGING!"
#endif
#endif

#include "config.h"

#define NOSORT 0

#define MAX_THREADS 32

#define HASHTABLE_SIZE 512
#define HASHTABLE_THRESHOLD 16

// transaction performance counters
enum txp {
    // all logging levels
    txp_total_aborts = 0,
    txp_total_starts,
    txp_commit_time_aborts,
    txp_max_set,
    txp_hco,
    txp_hco_lock,
    txp_hco_invalid,
    txp_hco_abort,
    // DETAILED_LOGGING only
    txp_total_n,
    txp_total_r,
    txp_total_w,
    txp_total_searched,
    txp_total_check_read,
#if !PERF_LOGGING
    txp_count = 0
#elif !DETAILED_LOGGING
    txp_count = txp_hco_abort + 1
#else
    txp_count = txp_total_check_read + 1
#endif
};

template <int N> struct has_txp_struct {
    static bool test(int p) {
        return unsigned(p) < unsigned(N);
    }
};
template <> struct has_txp_struct<0> {
    static bool test(int) {
        return false;
    }
};
inline bool has_txp(int p) {
    return has_txp_struct<txp_count>::test(p);
}


#include "Interface.hh"
#include "TransItem.hh"

#include "util.hh"
#include "Logger.hh"

#define INIT_SET_SIZE 512


void reportPerf();
#define STO_SHUTDOWN() reportPerf()

struct __attribute__((aligned(128))) threadinfo_t {
  unsigned epoch;
  unsigned spin_lock;
  local_vector<std::pair<unsigned, std::function<void(void)>>, 8> callbacks;
  local_vector<std::pair<unsigned, void*>, 8> needs_free;
  // XXX(NH): these should be vectors so multiple data structures can register
  // callbacks for these
  std::function<void(void)> trans_start_callback;
  std::function<void(void)> trans_end_callback;
  TransactionTid::type last_commit_tid;
  
  uint64_t p_[txp_count];
  threadinfo_t() : epoch(), spin_lock() {
#if PERF_LOGGING
      for (int i = 0; i != txp_count; ++i)
          p_[i] = 0;
#endif
  }
  static bool p_is_max(int p) {
      return p == txp_max_set;
  }
  unsigned long long p(int p) {
      return has_txp(p) ? p_[p] : 0;
  }
  void inc_p(int p) {
      add_p(p, 1);
  }
  void add_p(int p, uint64_t n) {
      if (has_txp(p))
          p_[p] += n;
  }
  void max_p(int p, unsigned long long n) {
      if (has_txp(p) && n > p_[p])
          p_[p] = n;
  }
  void combine_p(int p, unsigned long long n) {
      if (has_txp(p)) {
          if (!p_is_max(p))
              p_[p] += n;
          else if (n > p_[p])
              p_[p] = n;
      }
  }
};


// TransactionBuffer
template <typename T> struct ObjectDestroyer {
    static void destroy(void* object) {
        ((T*) object)->~T();
    }
};

class TransactionBuffer {
    struct elt;
    struct item;

  public:
    TransactionBuffer()
        : e_() {
    }
    ~TransactionBuffer() {
        if (e_)
            hard_clear(true);
    }

    static constexpr size_t aligned_size(size_t x) {
        return (x + 7) & ~7;
    }

    template <typename T>
    void* pack_unique(T x) {
        return pack_unique(std::move(x), (typename Packer<T>::is_simple_type) 0);
    }
    template <typename T>
    void* pack(T x) {
        return pack(std::move(x), (typename Packer<T>::is_simple_type) 0);
    }

    void clear() {
        if (e_)
            hard_clear(false);
    }

  private:
    struct itemhdr {
        void (*destroyer)(void*);
        size_t size;
    };
    struct item : public itemhdr {
        char buf[0];
    };
    struct elthdr {
        elt* next;
        size_t pos;
        size_t size;
    };
    struct elt : public elthdr {
        char buf[0];
        void clear() {
            size_t off = 0;
            while (off < pos) {
                itemhdr* i = (itemhdr*) &buf[off];
                i->destroyer(i + 1);
                off += i->size;
            }
            pos = 0;
        }
    };
    elt* e_;

    item* get_space(size_t needed) {
        if (!e_ || e_->pos + needed > e_->size) {
            size_t s = std::max(needed, e_ ? e_->size * 2 : 256);
            elt* ne = (elt*) new char[sizeof(elthdr) + s];
            ne->next = e_;
            ne->pos = 0;
            ne->size = s;
            e_ = ne;
        }
        e_->pos += needed;
        return (item*) &e_->buf[e_->pos - needed];
    }

    template <typename T>
    void* pack(T x, int) {
        return Packer<T>::pack(x);
    }
    template <typename T>
    void* pack(T x, void*) {
        size_t isize = aligned_size(sizeof(itemhdr) + sizeof(T));
        item* space = this->get_space(isize);
        space->destroyer = ObjectDestroyer<T>::destroy;
        space->size = isize;
        new (&space->buf[0]) T(std::move(x));
        return &space->buf[0];
    }

    template <typename T>
    void* pack_unique(T x, int) {
        return Packer<T>::pack(x);
    }
    template <typename T>
    void* pack_unique(T x, void*);

    void hard_clear(bool delete_all);
};

template <typename T>
void* TransactionBuffer::pack_unique(T x, void*) {
    void (*destroyer)(void*) = ObjectDestroyer<T>::destroy;
    for (elt* e = e_; e; e = e->next) {
        size_t off = 0;
        while (off < e->pos) {
            item* i = (item*) &e->buf[off];
            if (i->destroyer == destroyer
                && ((Aliasable<T>*) &i->buf[0])->x == x)
                return &i->buf[0];
            off += i->size;
        }
    }
    return pack(std::move(x));
}



class Transaction {
public:
  static threadinfo_t tinfo[MAX_THREADS];
  static __thread int threadid;
  static unsigned global_epoch;
  
  
  static bool run_epochs;
  static __thread Transaction* __transaction;
  typedef TransactionTid::type tid_type;
  
  tid_type final_commit_tid;
  
  static TransactionTid::type _TID;
public:

  static Transaction& get_transaction() {
    if (!__transaction)
      __transaction = new Transaction();
    else
      __transaction->reset();
    return *__transaction;
  }


  static std::function<void(unsigned)> epoch_advance_callback;

  static threadinfo_t tinfo_combined() {
    threadinfo_t out;
    for (int i = 0; i != MAX_THREADS; ++i) {
        for (int p = 0; p != txp_count; ++p)
            out.combine_p(p, tinfo[i].p(p));
    }
    return out;
  }

  static void print_stats();


  static void acquire_spinlock(unsigned& spin_lock) {
    unsigned cur;
    while (1) {
      cur = spin_lock;
      if (cur == 0 && bool_cmpxchg(&spin_lock, cur, 1)) {
        break;
      }
      relax_fence();
    }
  }
  static void release_spinlock(unsigned& spin_lock) {
    spin_lock = 0;
    fence();
  }

  static void* epoch_advancer(void*) {
    // don't bother epoch'ing til things have picked up
    usleep(100000);
    while (run_epochs) {
      auto g = global_epoch;
      for (auto&& t : tinfo) {
        if (t.epoch != 0 && t.epoch < g){
          g = t.epoch;
        }
      }

      global_epoch = ++g;

      if (epoch_advance_callback)
        epoch_advance_callback(global_epoch);

      for (auto&& t : tinfo) {
        acquire_spinlock(t.spin_lock);
        auto deletetil = t.callbacks.begin();
        for (auto it = t.callbacks.begin(); it != t.callbacks.end(); ++it) {
          // TODO: check for overflow
          if ((int)it->first <= (int)g-2) {
            it->second();
            ++deletetil;
          } else {
            // callbacks are in ascending order so if this one is too soon of an epoch the rest will be too
            break;
          }
        }
        if (t.callbacks.begin() != deletetil) {
          t.callbacks.erase(t.callbacks.begin(), deletetil);
        }
        auto deletetil2 = t.needs_free.begin();
        for (auto it = t.needs_free.begin(); it != t.needs_free.end(); ++it) {
          // TODO: overflow
          if ((int)it->first <= (int)g-2) {
            free(it->second);
            ++deletetil2;
          } else {
            break;
          }
        }
        if (t.needs_free.begin() != deletetil2) {
          t.needs_free.erase(t.needs_free.begin(), deletetil2);
        }
        release_spinlock(t.spin_lock);
      }
      usleep(100000);
    }
    return NULL;
  }

  static void rcu_cleanup(std::function<void(void)> callback) {
    acquire_spinlock(tinfo[threadid].spin_lock);
    tinfo[threadid].callbacks.emplace_back(global_epoch, callback);
    release_spinlock(tinfo[threadid].spin_lock);
  }

  static void rcu_free(void *ptr) {
    acquire_spinlock(tinfo[threadid].spin_lock);
    tinfo[threadid].needs_free.emplace_back(global_epoch, ptr);
    release_spinlock(tinfo[threadid].spin_lock);
  }

#if PERF_LOGGING
  static void inc_p(int p) {
      add_p(p, 1);
  }
  static void add_p(int p, uint64_t n) {
      tinfo[threadid].add_p(p, n);
  }
  static void max_p(int p, unsigned long long n) {
      tinfo[threadid].max_p(p, n);
  }
#endif

#if PERF_LOGGING
#define INC_P(p) inc_p((p))
#define ADD_P(p, n) add_p((p), (n))
#define MAX_P(p, n) max_p((p), (n))
#else
#define INC_P(p) do {} while (0)
#define ADD_P(p, n) do {} while (0)
#define MAX_P(p, n) do {} while (0)
#endif
  static void wait_an_epoch() {
    const uint64_t e = global_epoch;
    fence();
    std::cout << "Waiting for next epoch" << std::endl;
    while(global_epoch == e) { 
      nop_pause;
    }  
   
    std::cout << "Done waiting for next epoch" << std::endl;
  }

  static void sync() {
    wait_an_epoch();
    if (Logger::IsPersistenceEnabled()) { 
      Logger::wait_until_current_point_persisted();
    }
  }

  static void finish() {
   if (Logger::IsPersistenceEnabled()) {
     Logger::wait_until_current_point_persisted();
   }
  }
  
  static std::tuple<uint64_t, uint64_t, double>
  compute_ntxn_persisted() {
    if (!Logger::IsPersistenceEnabled())
      return std::make_tuple(0, 0, 0.0);
    return Logger::compute_ntxns_persisted_statistics();
  }

  static void reset_ntxn_persisted() {
    if (!Logger::IsPersistenceEnabled())
      return;
    Logger::clear_ntxns_persisted_statistics();
  }

  Transaction() : transSet_() {
    reset();
  }

  ~Transaction() {
    if (!isAborted_ && !transSet_.empty()) {
      silent_abort();
    }
    end_trans(false);
  }

  void end_trans(bool unlock) {
    // TODO: this will probably mess up with nested transactions
    tinfo[threadid].epoch = 0;
    if (tinfo[threadid].trans_end_callback) tinfo[threadid].trans_end_callback();
    if (unlock && Logger::IsPersistenceEnabled()) {
      Logger::persist_ctx &ctx = Logger::persist_ctx_for(threadid, Logger::INITMODE_REG);
      ctx.unlock();
    }
  }

  // reset data so we can be reused for another transaction
  void reset() {
     //if (isAborted_
     //   && tinfo[threadid].p(txp_total_aborts) % 0x10000 == 0xFFFF)
        //print_stats();
    tinfo[threadid].epoch = global_epoch;
    if (tinfo[threadid].trans_start_callback) tinfo[threadid].trans_start_callback();
    if (Logger::IsPersistenceEnabled()) {
      Logger::persist_ctx &ctx = Logger::persist_ctx_for(threadid, Logger::INITMODE_REG);
      ctx.lock();
    }
    transSet_.clear();
    writeset_ = NULL;
    nwriteset_ = 0;
    nhashed_ = 0;
    may_duplicate_items_ = false;
    isAborted_ = false;
    firstWrite_ = -1;
    start_tid_ = commit_tid_ = 0;
    buf_.clear();
    INC_P(txp_total_starts);
    start_us_ = util::cur_time_us();
  }

private:
  void consolidateReads() {
    // TODO: should be stable sort technically, but really we want to use insertion sort
    auto first = transSet_.begin();
    auto last = transSet_.end();
    std::sort(first, last);
    // takes the first element of any duplicates which is what we want. that is, we want to verify
    // only the oldest read
    transSet_.erase(std::unique(first, last), last);
    may_duplicate_items_ = false;
  }

public:
  // adds item for a key that is known to be new (must NOT exist in the set)
  template <typename T>
  TransProxy new_item(Shared* s, T key) {
      void *xkey = buf_.pack(std::move(key));
      transSet_.emplace_back(s, xkey);
      return TransProxy(*this, transSet_.back());
  }

  // adds item without checking its presence in the array
  template <typename T>
  TransProxy fresh_item(Shared *s, T key) {
      may_duplicate_items_ = !transSet_.empty();
      transSet_.emplace_back(s, buf_.pack_unique(std::move(key)));
      return TransProxy(*this, transSet_.back());
  }

  // tries to find an existing item with this key, otherwise adds it
  template <typename T>
  TransProxy item(Shared* s, T key) {
      void* xkey = buf_.pack_unique(std::move(key));
      TransItem* ti = find_item(s, xkey, 0);
      if (!ti) {
          transSet_.emplace_back(s, xkey);
          ti = &transSet_.back();
      }
      return TransProxy(*this, *ti);
  }

  // gets an item that is intended to be read only. this method essentially allows for duplicate items
  // in the set in some cases
  template <typename T>
  TransProxy read_item(Shared *s, T key) {
      void* xkey = buf_.pack_unique(std::move(key));
      TransItem* ti = nullptr;
      if (firstWrite_ >= 0)
          ti = find_item(s, xkey, firstWrite_);
      if (!ti) {
          may_duplicate_items_ = !transSet_.empty();
          transSet_.emplace_back(s, xkey);
          ti = &transSet_.back();
      }
      return TransProxy(*this, *ti);
  }

  template <typename T>
  OptionalTransProxy check_item(Shared* s, T key) {
      void* xkey = buf_.pack_unique(std::move(key));
      return OptionalTransProxy(*this, find_item(s, xkey, 0));
  }

private:
  static int hash(Shared* s, void* key) {
      auto n = (uintptr_t) key;
      n += -(n <= 0xFFFF) & reinterpret_cast<uintptr_t>(s);
      //2654435761
      return ((n >> 4) ^ (n & 15)) % HASHTABLE_SIZE;
  }

  // tries to find an existing item with this key, returns NULL if not found
  TransItem* find_item(Shared* s, void* key, int delta) {
#if TRANSACTION_HASHTABLE
      if (transSet_.size() > HASHTABLE_THRESHOLD) {
          if (nhashed_ < transSet_.size())
              update_hash();
          uint16_t idx = hashtable_[hash(s, key)];
          if (!idx)
              return NULL;
          else if (transSet_[idx - 1].sharedObj() == s && transSet_[idx - 1].key_ == key)
              return &transSet_[idx - 1];
      }
#endif
      for (auto it = transSet_.begin() + delta; it != transSet_.end(); ++it) {
          INC_P(txp_total_searched);
          if (it->sharedObj() == s && it->key_ == key)
              return &*it;
      }
      return NULL;
  }

private:
  typedef int item_index_type;
  item_index_type item_index(TransItem& ti) {
      return &ti - transSet_.begin();
  }

  void mark_write(TransItem& ti) {
    item_index_type idx = item_index(ti);
    if (firstWrite_ < 0 || idx < firstWrite_)
      firstWrite_ = idx;
  }

  bool check_for_write(const TransItem& item) const {
    // if writeset_ is NULL, we're not in commit (just an opacity check), so no need to check our writes (we
    // haven't locked anything yet)
    if (!writeset_)
      return false;
    auto it = &item;
    bool has_write = it->has_write();
    if (!has_write && may_duplicate_items_) {
      has_write = std::binary_search(writeset_, writeset_ + nwriteset_, -1, [&] (const int& i, const int& j) {
	  auto& e1 = unlikely(i < 0) ? item : transSet_[i];
	  auto& e2 = likely(j < 0) ? item : transSet_[j];
	  auto ret = likely(e1.key_ < e2.key_) || (unlikely(e1.key_ == e2.key_) && unlikely(e1.sharedObj() < e2.sharedObj()));
#if 0
	  if (likely(i >= 0)) {
	    auto cur = &i;
	    int idx;
	    if (ret) {
	      idx = (cur - writeset_) / 2;
	    } else {
	      idx = (writeset_ + nwriteset_ - cur) / 2;
	    }
	    __builtin_prefetch(&transSet_[idx]);
	  }
#endif
	  return ret;
	});
    }
    return has_write;
  }

  public:
  void check_reads() {
      if (!check_reads(transSet_.begin(), transSet_.end())) {
          abort();
      }
  }

  private:
  bool check_reads(const TransItem *trans_first, const TransItem *trans_last) const {
    for (auto it = trans_first; it != trans_last; ++it)
      if (it->has_read()) {
        INC_P(txp_total_check_read);
        if (!it->sharedObj()->check(*it, *this)) {
          // XXX: only do this if we're dup'ing reads
            for (auto jt = trans_first; jt != it; ++jt)
                if (*jt == *it)
                    goto ok;
            return false;
        }
      ok: ;
      }
    return true;
  }
  
  /*
  // Generate a tid for this transaction
  tid_t genCommitTid(TransSet transSet) {
    threadinfo_t &ctx = tinfo[threadid];
    uint64_t commit_epoch;
    
    // Get the current global epoch
    fence();
    commit_epoch = global_epoch;
    //std::cout<<"Commit epoch "<<commit_epoch<<std::endl;
    fence();
    
    tid_t ret = ctx.last_commit_tid;
    assert(commit_epoch >= epochId(ret));
    if (commit_epoch != epochId(ret))
      ret = makeTID(threadid, 0, commit_epoch);
    assert(ret >= ctx.last_commit_tid);
    
    {
      TransItem* trans_first = &transSet[0];
      TransItem* trans_last = trans_first + transSet.size();
      int i = 0;
      for (auto it = trans_first; it != trans_last; ++it) {
        // Need to get the tid from the underlying shared object.
        const tid_t t = it->sharedObj()->getTid(*it);
        assert (commit_epoch >= epochId(t));
        if (t > ret)
          //std::cout << "greater tid " << t << std::endl;
          ret = t;
      }
      //assert(ret >= ctx.last_commit_tid);
      assert(commit_epoch >= epochId(ret));
      ret = makeTID(threadid, numId(ret) + 1, commit_epoch);
    }
    if (ret <= ctx.last_commit_tid)
      ret = ctx.last_commit_tid + 1;
    return (ctx.last_commit_tid = ret);
  }*/
  
  void on_tid_finish(uint64_t commitEpoch, tid_type commitTid, bool success) {
    Transaction::tinfo[threadid].last_commit_tid = commitTid;
    if (!success) {
      return;
    } else if (!Logger::IsPersistenceEnabled()) {
      return;
    }
    
    Logger::persist_ctx &ctx = Logger::persist_ctx_for(threadid, Logger::INITMODE_REG);
    Logger::persist_stats &stats = Logger::g_persist_stats[threadid];   
 
    Logger::pbuffer_circbuf &pull_buf = ctx.all_buffers_;
    Logger::pbuffer_circbuf &push_buf = ctx.persist_buffers_;
    
    auto &epoch_stat = stats.epochStats[commitEpoch % Logger::g_max_lag_epochs]; 

    this->final_commit_tid = commitTid;
    //compute how much space is necessary
    uint64_t space_needed = 0;
    
    //8 bytes to indicate TID
    space_needed += sizeof(tid_type);
    //std::cout<<"Space needed after tid "<<space_needed<<std::endl;
    const unsigned nwrites = nwriteset_;
    space_needed += sizeof(nwrites);
    //std::cout<<"Space needed after nwrites "<<space_needed<<std::endl;
    // each record needs to be recorded
    TransItem* trans_first = &transSet_[0];
    TransItem* trans_last = trans_first + transSet_.size();
    for (auto it = trans_first + firstWrite_; it != trans_last; ++it) {
      TransItem& ti = *it;
      if (ti.has_write()) {
        // Call the shared object to get square required
        space_needed += ti.sharedObj()->spaceRequired(ti);
        //std::cout<<"Space needed after write "<<space_needed<<std::endl;
      }
    }
    assert(space_needed <= Logger::g_horizon_buffer_size);
    assert(space_needed <= Logger::g_buffer_size);
    
    util::non_atomic_fetch_add_(stats.ntxns_committed_, 1UL);
    
  retry:
    Logger::pbuffer *px = Logger::wait_for_head(pull_buf);
    assert(px && px->thread_id_ == threadid);
    
    bool epochChanged = false;
    if (px->space_remaining() < space_needed || (epochChanged = !px->can_hold_epoch(commitEpoch))) {
      assert(px->header()->nentries_);
      // Send the buffer to logger's queue
      Logger::pbuffer *px0 = pull_buf.deq();
      //std::cout << "pull buffer " << px0 << " dequeued by thread " << threadid <<std::endl;
      assert(px == px0);
      assert(px0->header()->nentries_);
      util::non_atomic_fetch_add_(stats.ntxns_pushed_, px0->header()->nentries_);
      push_buf.enq(px0);
      fence(); // TODO: Is this required?
      goto retry;
    }
    
    const uint64_t written = write_current_txn_into_buffer(px, commitTid, commitEpoch);
    //std::cout<<"Written = "<<written<<" and space needed = "<<space_needed<<std::endl;
    assert(written == space_needed);
  }
  
  inline uint64_t write_current_txn_into_buffer(Logger::pbuffer *px, uint64_t commitTid, uint64_t commitEpoch) {
    assert(px->can_hold_epoch(commitEpoch));
   
    if (!px->header()->nentries_) //First transaction
      px->earliest_txn_start_time_ = start_us_;
    px->last_txn_start_time_ = start_us_;
 
    uint8_t* p = px->pointer();
    uint8_t *porig = p;
    
    const unsigned nwrites = nwriteset_;
    
    p = write(p, commitTid);
    //std::cout<<"After tid "<<uint64_t(p-porig)<<std::endl;
    p = write(p, nwrites);
    //std::cout<<"After nwrites "<<uint64_t(p-porig)<<std::endl;
    TransItem* trans_first = &transSet_[0];
    TransItem* trans_last = trans_first + transSet_.size();
    for (auto it = trans_first + firstWrite_; it != trans_last; ++it) {
      TransItem& ti = *it;
      if (ti.has_write()) {
        p = ti.sharedObj()->writeLogData(ti, p);
        //std::cout<<"After write "<<uint64_t(p-porig)<<std::endl;
        
      }
    }
    
    px->cur_offset_ += (p - porig);
    px->header()->nentries_++;
    px->header()->last_tid_ = commitTid;
    px->header()->epoch = commitEpoch;
    
    return uint64_t(p - porig);
  }

  public:
  bool try_commit() {
#if ASSERT_TX_SIZE
    if (transSet_.size() > TX_SIZE_LIMIT) {
        std::cerr << "transSet_ size at " << transSet_.size()
            << ", abort." << std::endl;
        assert(false);
    }
#endif
    MAX_P(txp_max_set, transSet_.size());
    ADD_P(txp_total_n, transSet_.size());

    if (isAborted_)
      return false;

    bool success = true;
    tid_type commitTid = 0;
    uint64_t commitEpoch = 0;

    if (firstWrite_ < 0)
        firstWrite_ = transSet_.size();

    int writeset_alloc[transSet_.size() - firstWrite_];
    writeset_ = writeset_alloc;
    nwriteset_ = 0;
    for (auto it = transSet_.begin(); it != transSet_.end(); ++it) {

      if (it->has_write()) {
        writeset_[nwriteset_++] = it - transSet_.begin();
      }
#ifdef DETAILED_LOGGING
      if (it->has_read()) {
	INC_P(txp_total_r);
      }
#endif
    }

    //phase1
#if !NOSORT
    std::sort(writeset_, writeset_ + nwriteset_, [&] (int i, int j) {
	return transSet_[i] < transSet_[j];
      });
#endif
    TransItem* trans_first = &transSet_[0];
    TransItem* trans_last = trans_first + transSet_.size();

    auto writeset_end = writeset_ + nwriteset_;
    for (auto it = writeset_; it != writeset_end; ) {
      TransItem *me = &transSet_[*it];
      me->sharedObj()->lock(*me);
      ++it;
      if (may_duplicate_items_)
          for (; it != writeset_end && transSet_[*it].same_item(*me); ++it)
              /* do nothing */;
    }

    //Generate a commit tid right after phase 1
    if (Logger::IsPersistenceEnabled()) {
      commitTid = commit_tid();
      commitEpoch = global_epoch;
      fence();
    }
  
    //phase2
    if (!check_reads(trans_first, trans_last)) {
      success = false;
      goto end;
    }

    //    fence();

    //phase3
    for (auto it = trans_first + firstWrite_; it != trans_last; ++it) {
      TransItem& ti = *it;
      if (ti.has_write()) {
        INC_P(txp_total_w);
        ti.sharedObj()->install(ti, *this);
      }
    }

  end:
    //    fence();

    for (auto it = writeset_; it != writeset_end; ) {
      TransItem *me = &transSet_[*it];

      me->sharedObj()->unlock(*me);
      ++it;
      if (may_duplicate_items_)
          for (; it != writeset_end && transSet_[*it].same_item(*me); ++it)
              /* do nothing */;
    }

    //    fence();
    // Do logging at this point after releasing all locks
    if (Logger::IsPersistenceEnabled())
      on_tid_finish(commitEpoch, commitTid, success);


    if (success) {
      commitSuccess();
    } else {
      INC_P(txp_commit_time_aborts);
      silent_abort();
    }

    // Nate: we need this line because the Transaction destructor decides
    // whether to do an abort based on whether transSet_ is empty (meh)
    transSet_.clear();

    return success;
  }

  void silent_abort() {
    if (isAborted_)
      return;
    INC_P(txp_total_aborts);
    isAborted_ = true;
    for (auto& ti : transSet_) {
      ti.sharedObj()->cleanup(ti, false);
    }
    end_trans(true);
  }

  void abort() {
    silent_abort();
    //throw Abort();
  }

    void commit() {
        if (!try_commit())
            throw Abort();
    }

  bool aborted() {
    return isAborted_;
  }


    // opacity checking
    void check_opacity(TransactionTid::type t) {
        assert(!writeset_);
        if (!start_tid_)
            start_tid_ = _TID;
        if (!TransactionTid::try_check_opacity(start_tid_, t))
            hard_check_opacity(t);
    }

    tid_type commit_tid() const {
        assert(writeset_);
        if (!commit_tid_)
            commit_tid_ = fetch_and_add(&_TID, TransactionTid::increment_value);
        return commit_tid_;
    }

    class Abort {};

private:

  void commitSuccess() {
    for (TransItem& ti : transSet_) {
      ti.sharedObj()->cleanup(ti, true);
    }
    end_trans(true);
  }

private:
    int firstWrite_;
    bool may_duplicate_items_;
    bool isAborted_;
    uint16_t nhashed_;
    TransactionBuffer buf_;
    local_vector<TransItem, INIT_SET_SIZE> transSet_;
    int* writeset_;
    int nwriteset_;
    mutable tid_type start_tid_;
    mutable tid_type commit_tid_;
    uint16_t hashtable_[HASHTABLE_SIZE];
    uint64_t start_us_;    

    friend class TransProxy;
    friend class TransItem;
    void hard_check_opacity(TransactionTid::type t);
    void update_hash();
};

template <typename T>
inline TransProxy& TransProxy::add_read(T rdata) {
    if (!has_read()) {
        i_->__or_flags(TransItem::read_bit);
        i_->rdata_ = t_->buf_.pack(std::move(rdata));
    }
    return *this;
}

template <typename T, typename U>
inline TransProxy& TransProxy::update_read(T old_rdata, U new_rdata) {
    if (has_read() && this->read_value<T>() == old_rdata)
        i_->rdata_ = t_->buf_.pack(std::move(new_rdata));
    return *this;
}

template <typename T>
inline TransProxy& TransProxy::add_write(T wdata) {
    if (has_write())
        // TODO: this assumes that a given writer data always has the same type.
        // this is certainly true now but we probably shouldn't assume this in general
        // (hopefully we'll have a system that can automatically call destructors and such
        // which will make our lives much easier)
        this->template write_value<T>() = std::move(wdata);
    else {
        i_->__or_flags(TransItem::write_bit);
        i_->wdata_ = t_->buf_.pack(std::move(wdata));
        t_->mark_write(*i_);
    }
    return *this;
}

inline bool TransItem::has_lock(const Transaction& t) const {
    return t.check_for_write(*this);
}

inline bool TransProxy::has_lock() const {
    return t_->check_for_write(*i_);
}
