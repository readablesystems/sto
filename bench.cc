
#include "Mode.hh"

//volatile uint64_t anInt;
volatile Mode m;

inline __attribute__((always_inline)) void incr(volatile uint64_t *anInt) {
  //  m.acquire(255);
  *anInt += 1;
  //  m.release(255);
}

inline __attribute__((always_inline)) void fast_incr(volatile uint64_t *anInt) {
  *anInt += 1;
}

int main() {
  //m = Mode(0);
  m.owner_ = 255;

  volatile uint64_t anInt;

  //  for (anInt = 0; anInt < 1000000000; incr(&anInt)) ;
  for (int i = 0; i < 1000000000; incr(&anInt), i++) ;
  return anInt;
}
