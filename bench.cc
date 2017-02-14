
#include "Mode.hh"

//volatile uint64_t anInt;
Mode m;

inline __attribute__((always_inline)) void incr(volatile uint64_t *anInt, int threadid) {
  m.acquire(threadid);
  *anInt += 1;
  m.release(threadid);
  //  __sync_fetch_and_add(anInt, 1);
}

inline __attribute__((always_inline)) void fast_incr(volatile uint64_t *anInt) {
  *anInt += 1;
}

int main(int argc, char *argv[]) {
  //m = Mode(0);
  auto threadid = argc == 2 ? atoi(argv[1]) : 1;
  m.owner_ = threadid;

  volatile uint64_t anInt = 0;

  //  for (anInt = 0; anInt < 1000000000; incr(&anInt)) ;
  for (int i = 0; i < 1000000000; incr(&anInt, threadid), i++) ;
  return anInt;
}
