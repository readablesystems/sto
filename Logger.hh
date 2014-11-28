#pragma once

#include <iostream>
#include <atomic>
#include "util.hh"
#include "macros.hh"
#include "circbuf.hh"
#include "spinlock.hh"


class Logger {
  
public:
  friend class Transaction;
  static const size_t g_nmax_loggers = 16;
  static const size_t g_perthread_buffers = 512; // Outstanding buffers
  static const size_t g_buffer_size = (1 << 20); // in bytes
  static const size_t g_horizon_buffer_size = 1 << 17; // in bytes
  static const size_t g_max_lag_epochs = 128; // use to stop logging for threads that are producing log faster
  // static const bool g_pin_loggers_to_numa_nodes = false;
  
  // Public data structures
  struct logbuf_header {
    uint64_t nentries_; // > 0 for all valid log buffers
    uint64_t last_tid_; // TID of the last commit
    
    /*inline void set_entries(uint64_t entries) {
      nentries_ = entries;
    }
    
    inline void set_last_tid(uint64_t last_tid) {
      last_tid_ = last_tid;
    }*/
  } PACKED;
  
  // Buffer
  struct pbuffer {
    uint64_t earliest_txn_start_time_; // start time of the earliest txn
    uint64_t last_txn_start_time_; // start time of last txn
    bool io_scheduled_; // has the logger schedules the IO yet?
    unsigned cur_offset_; // current offset into the buffer for writing
    
    const unsigned thread_id_; // thread id of this buffer
    const unsigned buffer_size_;
    uint8_t buffer_start_[0];
    
    pbuffer(unsigned thread_id, unsigned buffer_size) : thread_id_(thread_id), buffer_size_(buffer_size) {
      assert(((char *)this) + sizeof(*this) == (char *) &buffer_start_[0]);
      assert(buffer_size > sizeof(logbuf_header));
      reset();
    }
    
    pbuffer(const pbuffer &) = delete;
    pbuffer &operator=(const pbuffer &) = delete;
    pbuffer(pbuffer &&) = delete;
    
    inline void reset() {
      earliest_txn_start_time_ = 0;
      last_txn_start_time_ = 0;
      io_scheduled_ = false;
      cur_offset_ = sizeof(logbuf_header);
      memset(&buffer_start_[0], 0, buffer_size_);
    }
    
    inline logbuf_header* header() {
      return reinterpret_cast<logbuf_header*>(&buffer_start_[0]);
    }
    
    inline const logbuf_header* header() const{
      return reinterpret_cast<const logbuf_header*>(&buffer_start_[0]);
    }
    
    inline size_t space_remaining() const {
      assert(cur_offset_ >= sizeof(logbuf_header));
      assert(cur_offset_ <= buffer_size_);
      return buffer_size_ - cur_offset_;
    }
    
    inline bool can_hold_tid(uint64_t tid) const {
      return !header()->nentries_ || epochId(header()->last_tid_) == epochId(tid);
    }
    
    inline uint8_t* pointer() {
      assert(cur_offset_ >= sizeof(logbuf_header));
      assert(cur_offset_ <= buffer_size_);
      return &buffer_start_[0] + cur_offset_;
    }
    
  };
  
  // init the logging subsystem.
  //
  // should only be called ONCE is not thread-safe.  if assignments_used is not
  // null, then fills it with a copy of the assignment actually computed
  static void Init(size_t nworkers, const std::vector<std::string> &logfiles, const std::vector<std::vector<unsigned>> &assignments_given, std::vector<std::vector<unsigned>> *assignments_used = nullptr, bool call_fsync = true, bool use_compression = false, bool fake_writes = false);
  
  static inline bool IsPersistenceEnabled() {
    return g_persist;
  }
  
  static inline bool IsCompressionEnabled() {
    return g_use_compression;
  }
  
  typedef circbuf<pbuffer, g_perthread_buffers> pbuffer_circbuf;
  
  
private:
  // data structures
  
  struct epoch_array {
    std::atomic<uint64_t> epochs_[MAX_THREADS_];
    std::atomic<uint64_t> dummy_work_; // so we can do some fake work
    CACHE_PADOUT;
  };
  
  struct persist_ctx {
    bool init_;
    void *lz4ctx_;      // for compression
    pbuffer *horizon_;  // for compression
    circbuf<pbuffer, g_perthread_buffers> all_buffers_; // buffers for thread
    circbuf<pbuffer, g_perthread_buffers> persist_buffers_; // buffers for logger
    spinlock lock_;
    persist_ctx() : init_(false), lz4ctx_(nullptr), horizon_(nullptr) {}
  };
  
  static void advance_system_sync_epoch(const std::vector<std::vector<unsigned>> &assignments);
  
  static void writer(unsigned id, std::string logfile, std::vector<unsigned> assignment);
  
  static void persister(std::vector<std::vector<unsigned>> assignments);
  
  
  enum InitMode{
    INITMODE_NONE, // no initialization
    INITMODE_REG, // use malloc to init buffers
    INITMODE_RCU, // try to use the RCU numa aware allocator
  };
  
  // Get persist_ctx for a thread
  static inline persist_ctx & persist_ctx_for(uint64_t thread_id, InitMode imode) {
    assert(thread_id < MAX_THREADS_);
    persist_ctx &ctx = g_persist_ctxs[thread_id];
    
    if (!ctx.init_  && imode != INITMODE_NONE) {
      size_t needed = g_perthread_buffers * (sizeof(pbuffer) + g_buffer_size);
      // TODO: deal with compression enabled case later
      
      // TODO: deal with rcu based allocation later
      char *mem = (char *) malloc(needed);
      
      for (size_t i = 0; i < g_perthread_buffers; i++) {
        ctx.all_buffers_.enq(new (mem) pbuffer(thread_id, g_buffer_size));
        mem += sizeof(pbuffer) + g_buffer_size;
      }
      
      ctx.init_ = true;
    }
    return ctx;
  }
  
public:
  
  // Spin until there is something in the queue
  static inline pbuffer* wait_for_head(pbuffer_circbuf &pull_buf) {
    pbuffer* px;
    while(!(px = pull_buf.peek())) {
      nop_pause;
    }
    assert(!px->io_scheduled_);
    return px;
  }
  
private:
  // State variables
  static bool g_persist; // whether or not logging is enabled.
  static bool g_call_fsync; // whether or not fsync() needs to be called
  static bool g_use_compression; // whether or not to compress log buffers.
  static bool g_fake_writes; // whether or not to fake doing writed to measure pure overhead of disk.
  static size_t g_nworkers; // assignments are computed based on g_nworkers
                            // but a logger responsible for core i is really
                            // responsible for cores i + k * g_nworkers, for k
                            // >= 0
  
  static epoch_array per_thread_sync_epochs_[g_nmax_loggers] CACHE_ALIGNED;
  
  static util::aligned_padded_elem<std::atomic<uint64_t>> system_sync_epoch_ CACHE_ALIGNED;
  
  static persist_ctx g_persist_ctxs[MAX_THREADS_] CACHE_ALIGNED;

};

