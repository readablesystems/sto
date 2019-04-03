#include "PlatformFeatures.hh"

#if defined(__APPLE__) || MALLOC == 0
#elif MALLOC == 1
#include <jemalloc/jemalloc.h>
#elif MALLOC == 2
#include "rpmalloc/rpmalloc.h"
#endif

#include <pthread.h>

void allocator_init() {
#if defined(__APPLE__) || MALLOC == 0
    // Do nothing for the default allocator
    return;
#elif MALLOC == 1
    char const* val = nullptr;
    size_t len = sizeof(val);
    int r;
    r = mallctl("opt.metadata_thp", &val, &len, NULL, 0);
    if (r == 0)
        std::cout << "jemalloc metadata THP: " << std::string(val) << std::endl;

    len = sizeof(val);
    r = mallctl("opt.thp", &val, &len, NULL, 0);
    if (r == 0)
        std::cout << "jemalloc THP: " << std::string(val) << std::endl;
#elif MALLOC == 2
    rpmalloc_config_t config;
    memset(&config, 0, sizeof(config));
    config.page_size = 2048 * 1024;
    config.enable_huge_pages = 1;
    rpmalloc_initialize_config(&config);
#endif
}

void set_affinity(int runner_id) {
#if defined(__APPLE__)
    (void)runner_id;
    std::cerr << "Warning: macOS does not support pthread_set_affinity(), thread affinity not set."
              << std::endl;
#else
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    // This is for the SEAS machine
    int cpu_id = runner_id;

    // This is for the Georgia Tech machine
    //int cpu_id = 24 * (runner_id % 8) + (runner_id / 8);

    // This is for AWS m4.16xlarge instances (64 threads, 32 cores, 2 sockets)
    //int cpu_id = runner_id / 2 + 16 * (runner_id % 2) + (runner_id / 32) * 16;

    // This is for AWS m5.24xlarge instances (96 threads, 48 cores, 2 sockets)
    //int cpu_id = runner_id / 2 + 24 * (runner_id % 2) + (runner_id / 48) * 24;

    CPU_SET(cpu_id, &cpuset);
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
        abort();
    }
#endif
}
