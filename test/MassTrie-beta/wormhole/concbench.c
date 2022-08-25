/*
 * Copyright (c) 2018-2019  Wu, Xingbo <wuxb45@gmail.com>
 *
 * All rights reserved. No warranty, explicit or implicit, provided.
 */
#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdatomic.h>
#include "lib.h"
#include "kv.h"
#include "wh.h"

atomic_uint_least64_t __seqno = 0;
u64 __nth = 0;
struct kv ** __samples = NULL;
u64 __nkeys = 0;
atomic_uint_least64_t __tot = 0;
u64 __endtime = 0;

  static void *
kv_load_worker(struct wormhole * const wh)
{
  srandom_u64(time_nsec() * time_nsec());
  struct wormref * const ref = wormhole_ref(wh);
  const u64 seq = atomic_fetch_add(&__seqno, 1);
  const u64 n0 = __nkeys / __nth * seq;
  const u64 nz = (seq == (__nth - 1)) ? __nkeys : (__nkeys / __nth * (seq + 1));
  printf("load worker %lu %lu\n", n0, nz);
  for (u64 i = n0; i < nz; i++)
    wormhole_put(ref, __samples[i]);
  wormhole_unref(ref);
  return NULL;
}

  static void *
kv_probe_worker(struct wormhole * const wh)
{
  struct wormref * const ref = wormhole_ref(wh);
  struct kv * next = __samples[random_u64() % __nkeys];
  u64 rnext = random_u64() % __nkeys;
  u64 count = 0;
  u64 succ = 0;
#define BATCH ((10000))
  do {
    for (u64 i = 0; i < BATCH; i++) {
      // reading kv samples leads to unnecessary cache misses
      // use prefetch to minimize overhead on workload generation
      struct kv * const key = next;
      next = __samples[rnext];
      __builtin_prefetch(next, 0, 0);
      __builtin_prefetch(((u8 *)next) + 64, 0, 0);
      rnext = random_u64() % __nkeys;
      __builtin_prefetch(&(__samples[rnext]));

      // do probe
      // customize your benchmark: do a mix of wh operations with switch-cases
      const struct kref kref = kv_kref(key);
      if (wormhole_probe(ref, &kref))
        succ++;
    }
    count += BATCH;
  } while (time_nsec() < __endtime);
  if (count != succ)
    printf("count %lu success %lu\n", count, succ);
  (void)atomic_fetch_add(&__tot, count);
  wormhole_unref(ref);
  return NULL;
}

  int
main(int argc, char ** argv)
{
  if (argc < 3) {
    printf("usage: <words-file> <#keys> <#threads>\n");
    printf("  Get words.txt: wget https://github.com/dwyl/english-words/raw/master/words.txt\n");
    printf("  Example: %s words.txt 1000000 4\n", argv[0]);
    printf("  Better to use only one numa node with numactl -N 0\n");
    printf("  Better to run X thread on X cores\n");
    return 0;
  }

  char ** const words = malloc(sizeof(char *) * 1000000); // or `wc -l words.txt`
  u64 nr_words = 0;
  char * buf = malloc(8192);
  size_t bufsize = 8192;
  FILE * const fwords = fopen(argv[1], "r");
  if (fwords == NULL) {
    printf("open words file failed\n");
    return 0;
  }

  // read all words to words
  while (getline(&buf, &bufsize, fwords) > 0) {
    buf[strlen(buf)-1] = '\0';
    words[nr_words] = strdup(buf);
    nr_words++;
  }
  fclose(fwords);

  // generate keys
  const u64 nkeys = strtoull(argv[2], NULL, 10);
  struct kv ** const samples = malloc(sizeof(struct kv *) * nkeys);
  char * ss[6];
  for (u64 i = 0; i < nkeys; i++) {
    for (u64 j = 0; j < 6; j++)
      ss[j] = words[random() % nr_words];
    sprintf(buf, "%s %s %s %s %s %s!", ss[0], ss[1], ss[2], ss[3], ss[4], ss[5]);
    samples[i] = kv_create_str(buf, NULL, 0);
  }
  // free words & buf
  for (u64 i = 0; i < nr_words; i++)
    free(words[i]);
  free(words);
  free(buf);

  // load (4)
  __samples = samples;
  __nkeys = nkeys;
  struct wormhole * const wh = wormhole_create(NULL);
  __nth = 4;
  const u64 dtl = thread_fork_join(4, (void *)kv_load_worker, false, (void *)wh);
  printf("load x4 %.2lf mops\n", ((double)nkeys) * 1e3 / ((double)dtl));

  const u64 nth = strtoull(argv[3], NULL, 10);
  printf("probe with %lu threads. each round takes 3 seconds\n", nth);
  for (u64 i = 0; i < 3; i++) {
    __tot = 0;
    __endtime = time_nsec() + 3e9; // 3 sec
    const u64 dt = thread_fork_join(nth, (void *)kv_probe_worker, false, (void *)wh);
    const double mops = ((double)__tot) * 1e3 / ((double)dt);
    printf("probe x%lu %.2lf mops\n", nth, mops);
    sleep(1);
  }

  // final clean up for valgrind
  for (u64 i = 0; i < nkeys; i++)
    free(samples[i]);
  free(samples);
  wormhole_destroy(wh);
  return 0;
}
