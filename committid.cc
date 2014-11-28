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
  typedef int Key;
  typedef int Value;
  
  std::string log("./disk1");
  const std::vector<std::string> logfiles({log});
  const std::vector<std::vector<unsigned> > assignments_given;
  Logger::Init(1,logfiles, assignments_given, NULL, true, false, false);
  MassTrans<Value> h;
  h.thread_init();
  int threadid = Transaction::threadid;
  Transaction t;
  
  Value v1, v2, vunused;
  
  assert(h.transInsert(t, 0, 1));
  h.transPut(t, 1, 3);
  
  assert(t.commit());
  uint64_t tid_1 = Transaction::tinfo[threadid].last_commit_tid;
  
  Transaction tm;
  assert(h.transUpdate(tm, 1, 2));
  assert(tm.commit());
  
  uint64_t tid_2 = Transaction::tinfo[threadid].last_commit_tid;
  
  assert(tid_1 < tid_2);
}