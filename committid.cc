#include <iostream>
#include <assert.h>

#include "Array.hh"
#include "MassTrans.hh"
#include "Logger.hh"
#include "Transaction.hh"

kvepoch_t global_log_epoch = 0;
volatile uint64_t globalepoch = 1;     // global epoch, updated by main thread regularly
kvtimestamp_t initial_timestamp;
volatile bool recovering = false; // so don't add log entries, and free old value immediately

int main() {
  
  Transaction::epoch_advance_callback = [] (unsigned) {
    // just advance blindly because of the way Masstree uses epochs
    globalepoch++;
  };
  
  typedef int Key;
  typedef int Value;
  
  std::string log("./disk1");
  const std::vector<std::string> logfiles({log});
  const std::vector<std::vector<unsigned> > assignments_given;
  
  pthread_t advancer;
  pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
  pthread_detach(advancer);
  
  Logger::Init(1,logfiles, assignments_given, NULL, true, false, false);
  MassTrans<Value> h;
  h.thread_init();
  int threadid = Transaction::threadid;
  Transaction t;
  
  Value v1, v2, vunused;
  
  h.transWrite(t, 0, 1);
  h.transWrite(t, 1, 3);
  
  assert(t.commit());
  uint64_t tid_1 = Transaction::tinfo[threadid].last_commit_tid;
  
  Transaction tm;
  h.transWrite(tm, 1, 2);
  assert(tm.commit());
  
  uint64_t tid_2 = Transaction::tinfo[threadid].last_commit_tid;
  
  assert(tid_1 < tid_2);
  usleep(200000); // Wait for some time for loggers to finish job
}