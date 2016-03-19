#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

#if __cplusplus__
extern "C" {
#endif
void srandomdev(void) {
    unsigned seed;
    size_t n = 0;
    FILE* f = fopen("/dev/urandom", "r");
    if (f) {
        n = fread(&seed, sizeof(seed), 1, f);
        fclose(f);
    }
    if (!n) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        seed = (getpid() ^ tv.tv_sec) + tv.tv_usec;
    }
    srandom(seed);
}
#if __cplusplus__
}
#endif
