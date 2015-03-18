#include <iostream>
#include <assert.h>
#include <thread>

#include "Transaction.hh"
#include "GenericSTM.hh"

volatile uint64_t globalepoch = 1;

volatile bool recovering = false;

typedef int Key;
typedef int Value;

int main() {
  Transaction::epoch_advance_callback = [] (unsigned) {
    globalepoch++;
  };
  
  pthread_t advancer;
  pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
  pthread_detach(advancer);
  
  GenericSTM stm_;
  int x = 5;
  Transaction t1;
  int x_ = stm_.transRead(t1, &x);
  assert(x_ == 5);
  
  Transaction t2;
  stm_.transWrite(t2, &x, 4);
  assert(t2.try_commit());
  assert(x == 4);
  
  
  assert(!t1.try_commit());
  
  
}
