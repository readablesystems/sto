#include <iostream>
#include <assert.h>

#include "Array.hh"
#include "MassTrans.hh"
#include "Logger.hh"
#include "Transaction.hh"
#include "Checkpointer.hh"

kvepoch_t global_log_epoch = 0;
volatile uint64_t globalepoch = 1;     // global epoch, updated by main thread regularly
kvtimestamp_t initial_timestamp;
volatile bool recovering = false; // so don't add log entries, and free old value immediately
extern std::vector<std::vector<std::string> > ckpdirs;

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
  MassTrans<Value> h;
  h.set_tree_id(1);
  
  std::vector<MassTrans<Value> *>tree_list;
  tree_list.push_back(&h);
  ckpdirs = {
    {"./f0/disk1/", "./f0/disk2/",
      "./f0/disk3/", "./f0/disk4/",
      "./f0/disk5/", "./f0/disk6/"},
    
    {"./f1/disk1/", "./f1/disk2/",
      "./f1/disk3/", "./f1/disk4/",
      "./f1/disk5/", "./f1/disk6/"},
    
    {"./f2/disk1/", "./f2/disk2/",
      "./f2/disk3/", "./f2/disk4/",
      "./f2/disk5/", "./f2/disk6/"},
    
    {"./data/disk1/", "./data/disk2/",
      "./data/disk3/", "./data/disk4/",
      "./data/disk5/", "./data/disk6/"},
  };
  
  Logger::Init(1,logfiles, assignments_given, NULL, true, false, false);
  Checkpointer::Init(&tree_list, logfiles, true);
  
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
  sleep(20);
}