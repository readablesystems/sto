
#include <iostream>
#include "Logger.hh"
#include <thread>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>

#include "Transaction.hh"


std::string root_folder; // this folder stores ptid, ctid and other information

bool Logger::g_persist = false;
bool Logger::g_call_fsync = true;
bool Logger::g_use_compression = false;
bool Logger::g_fake_writes = false;
size_t Logger::g_nworkers = 0;


Logger::tid_array Logger::per_thread_sync_tids_[Logger::g_nmax_loggers];
util::aligned_padded_elem<std::atomic<uint64_t>> Logger::system_sync_tid_(0);

Logger::persist_ctx Logger::g_persist_ctxs[MAX_THREADS_];
Logger::persist_stats Logger::g_persist_stats[MAX_THREADS_];

extern volatile uint64_t fsync_sync[NUM_TH_CKP];

void Logger::Init(size_t nworkers, const std::vector<std::string> &logfiles, const std::vector<std::vector<unsigned>> &assignments_given, std::vector<std::vector<unsigned>> *assignments_used, bool call_fsync, bool use_compression, bool fake_writes) {
  assert(!g_persist);
  assert(g_nworkers == 0);
  assert(nworkers > 0);
  assert(!logfiles.empty());
  assert(logfiles.size() <= g_nmax_loggers);
  assert(!use_compression || g_perthread_buffers > 1); // need one as scratch buffer
  
  g_persist = true;
  g_call_fsync = call_fsync;
  g_use_compression = use_compression;
  g_fake_writes = fake_writes;
  g_nworkers = nworkers;
  
  // Initialize per_thread_sync_tids
  for (size_t i = 0; i < g_nmax_loggers; i++)
    for (size_t j = 0; j < g_nworkers; j++)
      per_thread_sync_tids_[i].tids_[j].store(0, std::memory_order_release);
  
  std::vector<std::thread> writers;
  std::vector<std::vector<unsigned>> assignments(assignments_given);
  
  if (assignments.empty()) {
    if (g_nworkers <= logfiles.size()) {
      for (size_t i = 0; i < g_nworkers; i++) {
        assignments.push_back({(unsigned) i});
      }
    } else {
      const size_t threads_per_logger = g_nworkers / logfiles.size();
      for (size_t i = 0; i < logfiles.size(); i++) {
        assignments.emplace_back(MakeRange<unsigned>(i * threads_per_logger, ((i + 1) == logfiles.size()) ? g_nworkers : (i+1) * threads_per_logger));
      }
    }
  }
  
  // Assign logger threads
  for (size_t i = 0; i < assignments.size(); i++) {
    writers.emplace_back(Logger::writer, i, logfiles[i], assignments[i]);
    writers.back().detach();
  }
  
  // persist_thread is responsible for calling sync tid.
  std::thread persist_thread(&Logger::persister, assignments);
  persist_thread.detach();
  
  
  if (assignments_used)
    *assignments_used = assignments;
}

void Logger::persister(std::vector<std::vector<unsigned>> assignments) {
  for (;;) {
    usleep(40000); // sleep for 40 ms
    advance_system_sync_tid(assignments);
  }
}

void Logger::advance_system_sync_tid(const std::vector<std::vector<unsigned>> &assignments) {
  uint64_t min_so_far = std::numeric_limits<uint64_t>::max(); // Minimum of persist tids of all worker threads
  const uint64_t cur_tid = Transaction::_TID;
  const uint64_t best_tid = cur_tid;
  if (best_tid == 0)
    std::cout<<"best tid "<<Transaction::_TID <<std::endl;
  for (size_t i = 0; i < assignments.size(); i++) {
    for (auto j :assignments[i]) {
      for (size_t k = j; k < g_nworkers; k += g_nworkers) {
        // we need to arbitrarily advance threads which are not "doing
        // anything", so they don't drag down the persistence of the system. if
        // we can see that a thread is NOT in a guarded section AND its
        // logger queue is empty, then that means we can advance its sync
        // epoch up to best_epoch, b/c it is guaranteed that the next time
        // it does any actions will be in epoch > best_epoch

        
        // we also need to make sure that any outstanding buffer (should only have 1)
        // is written to disk
        
        persist_ctx &ctx = persist_ctx_for(k, INITMODE_NONE);
        
        if (!ctx.persist_buffers_.peek()) {
          // Need to lock the thread
          spinlock &l = ctx.lock_;
          if (!l.is_locked()) {
            bool did_lock = false;
            for (size_t c = 0; c < 3; c++) {
              if (l.try_lock()) {
                did_lock = true;
                break;
              }
            }
            
            if (did_lock) {
              pbuffer * last_px = ctx.all_buffers_.peek();
              if (last_px && last_px->header()->nentries_ > 0) {
                // Outstanding buffer; should remove it and add to the push buffers.
                
                Logger::persist_stats &stats = Logger::g_persist_stats[k];
                util::non_atomic_fetch_add_(stats.ntxns_pushed_, last_px->header()->nentries_);
                
                ctx.all_buffers_.deq();
                assert(!last_px->io_scheduled_);
                ctx.persist_buffers_.enq(last_px);
              }
              
              if (!ctx.persist_buffers_.peek()) {
                // If everything is written to disk and all buffers are clean, then increment tid for that worker.
                min_so_far = std::min(min_so_far, best_tid);
                per_thread_sync_tids_[i].tids_[k].store(best_tid, std::memory_order_release);
                l.unlock();
                continue;
              }
              l.unlock();
            }
          }
        }
        
        min_so_far = std::min(per_thread_sync_tids_[i].tids_[k].load(std::memory_order_acquire), min_so_far);
      }
    }
  }
  
  const uint64_t syssync = system_sync_tid_->load(std::memory_order_acquire);
  assert(min_so_far < std::numeric_limits<uint64_t>::max());
  assert(syssync <= min_so_far);
  
  // Stats
  const uint64_t now_us = util::cur_time_us();
  for (size_t i = 0; i < MAX_THREADS_; i++) {
    auto &ps = g_persist_stats[i];
    for (int j = 0 ; j < g_max_lag_epochs; j++) {
      auto &pes = ps.epochStats[j];
      const uint64_t last_tid = pes.last_tid_.load(std::memory_order_acquire);
      if (last_tid > 0 && last_tid <= min_so_far) {
        const uint64_t start_us = pes.start_time_.load(std::memory_order_acquire);
        const uint64_t last_us = pes.last_time_.load(std::memory_order_acquire);
        const uint64_t ntxns_persisted = pes.ntxns_.load(std::memory_order_acquire);

        if (start_us == 0 && last_us == 0)
          continue;
        if (now_us < start_us)
          std::cout << "Now " << now_us << " start " << start_us << std::endl;
        assert(now_us >= start_us);
        util::non_atomic_fetch_add_(ps.ntxns_persisted_, ntxns_persisted);
        util::non_atomic_fetch_add_(ps.total_latency_,
           (now_us - (last_us + start_us)/2) * ntxns_persisted);

        pes.ntxns_.store(0, std::memory_order_release);
        pes.start_time_.store(0, std::memory_order_release);
        pes.last_time_.store(0, std::memory_order_release);
        pes.last_tid_.store(0, std::memory_order_release);
      }
    }
  }
   
  // write the persistent tid to disk
  // to avoid failure during write, write to another file and then rename
    if (syssync < min_so_far) {
    std::string persist_tid_symlink = root_folder + "/ptid";
    std::string persist_tid_filename = root_folder + "/persist_tid_";
    
    persist_tid_filename.append(std::to_string(min_so_far));
    
    int fd = open((char*) persist_tid_filename.c_str(), O_WRONLY|O_CREAT, 0777);
    ssize_t tid_ret = write(fd, (char *) &min_so_far, 8);
    if (tid_ret == -1) {
      perror("Writing persist tid to disk failed");
      assert(false);
    }
    fsync(fd);
    close(fd);
    
    if (rename(persist_tid_filename.c_str(), persist_tid_symlink.c_str()) < 0) {
      perror("Renaming failure");
      assert(false);
    }
  }
  assert(min_so_far <= best_tid);
  //std::cout<<"Storing " << min_so_far << " in system_sync_epoch" << std::endl;
  system_sync_tid_->store(min_so_far, std::memory_order_release);
}

void Logger::writer(unsigned id, std::string logfile, std::vector<unsigned> assignment) {
  int fd = -1;
  uint64_t min_tid_so_far = 0;
  uint64_t max_tid_so_far = 0;
  
  std::vector<iovec> iovs(std::min(size_t(IOV_MAX), g_nworkers * g_perthread_buffers));
  std::vector<pbuffer*> pxs;
  
  uint64_t tid_prefixes[MAX_THREADS_];
  memset(&tid_prefixes, 0, sizeof(tid_prefixes));
  
  std::string logfile_name(logfile);
  logfile_name.append("data.log");
  
  size_t nbufswritten = 0, nbyteswritten = 0, totalbyteswritten = 0, totalbufswritten = 0;
  for(;;) {
    usleep(40000); // To support batch IO
    
    if (fd == -1 || max_tid_so_far - min_tid_so_far > 10000000 ) { // Magic number
      if (max_tid_so_far - min_tid_so_far > 10000000) {
        std::string fname(logfile);
        fname.append("old_data");
        fname.append(std::to_string(max_tid_so_far));
        
        if (rename(logfile_name.c_str(), fname.c_str()) < 0){
          perror("Renaming failure");
          assert(false);
        }
            
        close(fd);
      }
      
      fd = open (logfile_name.c_str(), O_CREAT|O_WRONLY|O_TRUNC, 0664);
      if (fd == -1) {
        perror("Log file open failure");
        assert(false);
      }
      
      min_tid_so_far = max_tid_so_far;
    }
    
    const uint64_t cur_sync_tid_ex = system_sync_tid_->load(std::memory_order_acquire) + 1;
    nbufswritten = nbyteswritten = totalbyteswritten = totalbufswritten = 0;
    
    //NOTE: a core id in the persistence system really represents
    //all cores in the regular system modulo g_nworkers
    for (auto idx : assignment) {
      assert(idx >=0 && idx < g_nworkers);
      for (size_t k = idx; k < g_nworkers; k+= g_nworkers) {
        persist_ctx &ctx = persist_ctx_for(k, INITMODE_NONE);
        ctx.persist_buffers_.peekall(pxs);
        
        for (auto px : pxs) {
          assert(px);
          assert(!px->io_scheduled_);
          assert(nbufswritten < iovs.size());
          assert(px->header()->nentries_);
          assert(px->thread_id_ == k);
          if (nbufswritten == iovs.size()) {
            // Writer limit met
            goto process;
          }
          
          if (px->header()->epoch >= cur_sync_tid_ex + g_max_lag_epochs) { // TODO: this is wrong - mixing up tid and epochs
            // logger max log wait
            break;
          }
          iovs[nbufswritten].iov_base = (void*) &px->buffer_start_[0];
          
#ifdef LOGGER_UNSAFE_REDUCE_BUFFER_SIZE
#define PXLEN(px) (((px)->cur_offset_ < 4) ? (px)->cur_offset_ : ((px)->cur_offset_ / 4))
#else
#define PXLEN(px) ((px)->cur_offset_)
#endif
          const size_t pxlen = PXLEN(px);
          iovs[nbufswritten].iov_len = pxlen;
          px->io_scheduled_ = true;
          nbufswritten++;
          nbyteswritten += pxlen;
          totalbyteswritten += pxlen;
          totalbufswritten++;
          
          const uint64_t px_tid = px->header()->last_tid_;
          const uint64_t px_epoch = px->header()->epoch;
          tid_prefixes[k] = px_tid == 0 ? 0 : px_tid - 1;
          
          max_tid_so_far = px_tid > max_tid_so_far ? px_tid : max_tid_so_far;
        
          auto &epoch_stat = g_persist_stats[k].epochStats[px_epoch % g_max_lag_epochs];
          if (!epoch_stat.ntxns_.load(std::memory_order_acquire)) {
            epoch_stat.start_time_.store(px->earliest_txn_start_time_, std::memory_order_release);
          }
          epoch_stat.last_time_.store(px->last_txn_start_time_, std::memory_order_release);
          epoch_stat.last_tid_.store(px_tid, std::memory_order_release);
          util::non_atomic_fetch_add_(epoch_stat.ntxns_, px->header()->nentries_);
        
        }
      }
      
    process:
      if (!g_fake_writes && nbufswritten > 0) {
        const ssize_t ret = writev(fd, &iovs[0], nbufswritten);
        if (ret == -1) {
          perror("writev");
          assert(false);
        }
        
        nbufswritten = nbyteswritten = 0;
        
        //after writev is called, buffers can be immediately returned to the workers
        for (size_t k = idx; k < MAX_THREADS_; k += g_nworkers) {
          persist_ctx &ctx = persist_ctx_for(k, INITMODE_NONE);
          pbuffer *px, *px0;
          while ((px = ctx.persist_buffers_.peek()) && px->io_scheduled_) {
            px0 = ctx.persist_buffers_.deq();
            assert(px == px0);
            assert(px->header()->nentries_);
            px0->reset();
            assert(ctx.init_);
            assert(px0->thread_id_ == k);
            assert(!px0->io_scheduled_);
            ctx.all_buffers_.enq(px0);
          }
        }
      }
    }
    
    if (!totalbufswritten) {
      // should probably sleep here
      __asm volatile("pause" : :);
      continue;
    }
    
    if (!g_fake_writes && g_call_fsync) {
      fsync_sync[id] = 1;
      int fret = fsync(fd);
      if (fret < 0) {
        perror("fsync logger failed\n");
        assert(false);
      }
      fsync_sync[id] = 0;
    }
    
    tid_array &ea = per_thread_sync_tids_[id];
    for (auto idx : assignment) {
      for (size_t k = idx; k < MAX_THREADS_; k += g_nworkers) {
        const uint64_t x0 = ea.tids_[k].load(std::memory_order_acquire);
        const uint64_t x1 = tid_prefixes[k];
        if (x1 > x0) {
         // std::cout<<"Putting into per thread epoch "<<x1<<std::endl;
          ea.tids_[k].store(x1, std::memory_order_release);
        }
      }
    }
  }
}

std::tuple<uint64_t, uint64_t, double> Logger::compute_ntxns_persisted_statistics() {
  uint64_t pers = 0, push = 0, comm = 0, latency = 0.0;
  for (size_t i = 0; i < MAX_THREADS_; i++) {
    pers += g_persist_stats[i].ntxns_persisted_.load(std::memory_order_acquire);
    push += g_persist_stats[i].ntxns_pushed_.load(std::memory_order_acquire);
    latency += g_persist_stats[i].total_latency_.load(std::memory_order_acquire);
    comm += g_persist_stats[i].ntxns_committed_.load(std::memory_order_acquire);
  }
  std::cout << "pers " << pers << " push" << push << " comm " << comm <<  std::endl;
  assert(pers <= push);
  if (pers == 0)
    return std::make_tuple(0, push, 0.0);
  std::cout << "Latency " << (double(latency)/double (pers)) << std::endl;
  std::cout << "Pers " << pers << std::endl;
  return std::make_tuple(pers, push, double(latency) / double(pers));
}

void Logger::clear_ntxns_persisted_statistics() {
  for (size_t i = 0; i < MAX_THREADS_; i++) {
    auto &ps = g_persist_stats[i];
    ps.ntxns_persisted_.store(0, std::memory_order_release);
    ps.ntxns_pushed_.store(0, std::memory_order_release);
    ps.ntxns_committed_.store(0, std::memory_order_release);
    ps.total_latency_.store(0, std::memory_order_release);
    for (size_t e = 0; e < g_max_lag_epochs; e++) {
      auto &pes = ps.epochStats[e];
      pes.ntxns_.store(0, std::memory_order_release);
      pes.start_time_.store(0, std::memory_order_release);
      pes.last_tid_.store(0, std::memory_order_release);
    }
  }
}

void Logger::wait_for_idle_state() {
  for (size_t i = 0; i < MAX_THREADS_; i++) {
    persist_ctx &ctx = persist_ctx_for(i, INITMODE_NONE);
    if (!ctx.init_)
      continue;
    pbuffer *px;
    while (!(px = ctx.all_buffers_.peek()) || px->header()->nentries_)
      __asm volatile("pause" : :);
    while (ctx.persist_buffers_.peek())
      __asm volatile("pause" : :);
  }
}

void Logger::wait_until_current_point_persisted() {
  const uint64_t t = Transaction::_TID;
  fence();
  std::cout << "Waiting until current point is persisted " << t << " epoch point is " << system_sync_tid_->load(std::memory_order_acquire) << std::endl;
  while(system_sync_tid_->load(std::memory_order_acquire) < t) {
    nop_pause;
    //std::cout << system_sync_tid_->load(std::memory_order_acquire) << std::endl;
  }
  const uint64_t tt = Transaction::_TID;
  fence();
  std::cout << tt << " is persisted!" << std::endl;

  for (size_t i = 0; i < MAX_THREADS_; i++) {
    persist_ctx &ctx = persist_ctx_for(i, INITMODE_NONE);
    if (!ctx.init_)
      continue;
    pbuffer *px;
    if (!(px = ctx.all_buffers_.peek()) || px->header()->nentries_)
      {std::cout << "Outstanding commits that are not pushed to logger " << i << std::endl;
      std::cout << (px->header()->nentries_) << std::endl;
      std::cout << (px->header()->last_tid_) << std::endl; 
    }
    if (ctx.persist_buffers_.peek())
      std::cout << "Outstanding commits that should be written to disk" << std::endl;
  }

}
