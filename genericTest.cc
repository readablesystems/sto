#include <iostream>
#include <assert.h>
#include <thread>

#include "Transaction.hh"
#include "GenericSTM.hh"

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
  Transaction t1(Transaction::testing);
  Sto::set_transaction(&t1);
  int x_ = stm_.transRead(&x);
  assert(x_ == 5);
  
  Transaction t2(Transaction::testing);
  Sto::set_transaction(&t2);
  stm_.transWrite(&x, 4);
  assert(t2.try_commit());
  assert(x == 4);
  
  
  assert(!t1.try_commit());
  
  
}
