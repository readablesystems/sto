#include <sys/time.h>
#include <sys/resource.h>
#include <random>

#include "randgen.hh"
#include "Transaction.hh"
#include "SwissTGeneric.hh"

#define ARRAY_SZ 1000

#define NON_CONFLICTING 1

int nthreads = 4;
int ntrans = 100000000;
int opspertrans = 100;
double write_percent = 0.5;

unsigned initial_seeds[64];

typedef struct {
  int thread_id;
  //Word *array;
  SwissTNonopaqueGeneric *stm;
  int *array;
} arg_t;

inline void doRead(SwissTNonopaqueGeneric *stm, int *array, int slot) {
  //wlpdstm_read_word(&array[slot]);
  stm->read(&array[slot], slot);
}

inline void doWrite(SwissTNonopaqueGeneric *stm, int *array, int slot) {
  //iWord v0 = wlpdstm_read_word(&array[slot]);
  //wlpdstm_write_word(&array[slot], v0 + 1);
  int val = stm->read(&array[slot], slot);
  stm->write(&array[slot], val + 1, slot);
}

void *RandomRWs(void *arg) {
  std::cout << "Here" << std::endl;
  arg_t *d = (arg_t *)arg;
  int me = d->thread_id;
  SwissTNonopaqueGeneric *stm = d->stm;
  int * array = d->array;

#if NON_CONFLICTING
  long range = ARRAY_SZ/nthreads;
  std::uniform_int_distribution<long> slotdist(me*range + 10, (me + 1) * range - 1 - 10);
#else
  std::uniform_int_distribution<long> slotdist(0, ARRAY_SZ-1);
#endif
  uint32_t write_thresh = (uint32_t) (write_percent * Rand::max());
  Rand transgen(initial_seeds[2*me], initial_seeds[2*me + 1]);

  int N = ntrans/nthreads;
  int OPS = opspertrans;

  //iwlpdstm_thread_init();

  for (int i = 0; i < N; ++i) {
    Rand transgen_snap = transgen;

    //BEGIN_TRANSACTION;
    TRANSACTION {

    transgen = transgen_snap;
    for (int j = 0; j < OPS; ++j) {
      int slot = slotdist(transgen);
      auto r = transgen();

      if (r > write_thresh) {
        doRead(stm, array, slot);
	//doWrite(array, slot);
	//++j;
      } else {
        //doWrite(array, slot);
        //++j;
	doRead(stm, array, slot);
      }
    }

    //END_TRANSACTION;
    } RETRY(true);
  }

  //wlpdstm_thread_shutdown();   
  return NULL;
}

void print_time(struct timeval tv1, struct timeval tv2) {
  printf("%f\n", (tv2.tv_sec - tv1.tv_sec) + (tv2.tv_usec - tv1.tv_usec) / 1000000.0);
}

int main() {
  for (unsigned i = 0; i < 64; ++i) {
    initial_seeds[i] = i;//rand();
  }

  //wlpdstm_global_init();
  //wlpdstm_thread_init();

  //std::cout << "P1" << std::endl;

  struct timeval tv1, tv2;
  struct rusage ru1, ru2;

  gettimeofday(&tv1, NULL);
  getrusage(RUSAGE_SELF, &ru1);

  pthread_t threads[nthreads];
  arg_t arg[nthreads];
  //iWord *array = (Word *)wlpdstm_tx_malloc(sizeof(Word) * ARRAY_SZ);
  SwissTNonopaqueGeneric stm;
  int array[ARRAY_SZ];
  for (int i = 0; i < nthreads; i++) {
    arg[i].thread_id = i;
    arg[i].stm = &stm;
    arg[i].array = array;
    pthread_create(&threads[i], NULL, RandomRWs, &arg[i]);
  }
  for (int i = 0; i < nthreads;i ++) {
    pthread_join(threads[i], NULL);
  }

  gettimeofday(&tv2, NULL);
  getrusage(RUSAGE_SELF, &ru2);

  printf("real time: ");
  print_time(tv1, tv2);
  printf("utime: ");
  print_time(ru1.ru_utime, ru2.ru_utime);
  printf("stime: ");
  print_time(ru1.ru_stime, ru2.ru_stime);

  //wlpdstm_thread_shutdown();
  //wlpdstm_global_shutdown();
  return 0;
}

