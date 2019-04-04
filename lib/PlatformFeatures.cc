#include "PlatformFeatures.hh"

#if defined(__APPLE__) || MALLOC == 0
#elif MALLOC == 1
#include <jemalloc/jemalloc.h>
#elif MALLOC == 2
#include "rpmalloc/rpmalloc.h"
#endif

#include <numa.h>
#include <pthread.h>

TopologyInfo topo_info;

void discover_topology() {
    TopologyInfo info {};

    int r = numa_available();
    if (r < 0) {
        std::cerr << "Error: libnuma not available, aborting." << std::endl;
        abort();
    }

    r = numa_num_configured_nodes();
    if (r < 0) {
        std::cerr << "Error: Cannot determine number of NUMA nodes in the system." << std::endl;
        abort();
    }
    std::cout << "[discover_topology] Detected number of NUMA nodes: " << r << std::endl;
    info.num_nodes = r;

    int max_cpus = numa_num_possible_cpus();
    auto cpu_mask = numa_allocate_cpumask();

    for (int i = 0; i < info.num_nodes; ++i) {
        std::vector<int> node_cpu_list;
        r = numa_node_to_cpus(i, cpu_mask);
        if (r < 0) {
            std::cerr << "Error: Cannot get CPU mask for NUMA node [" << i << "]." << std::endl;
            abort();
        }
        std::cout << "[discover_topology] CPUs on node [" << i << "]:" << std::endl;
        std::cout << "    ";
        bool first = true;
        for (int c = 0; c < max_cpus; ++c) {
            if (numa_bitmask_isbitset(cpu_mask, c) != 0) {
                if (!first) {
                    std::cout << ",";
                }
                std::cout << c;
                first = false;
                node_cpu_list.push_back(c);
            }
        }
        std::cout << std::endl << std::flush;
        numa_bitmask_clearall(cpu_mask);
        info.cpu_id_list.push_back(node_cpu_list);
    }

    numa_free_cpumask(cpu_mask);
    topo_info = info;
}

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
    int q = runner_id / topo_info.num_nodes;
    q = q % topo_info.cpu_id_list[0].size();
    int r = runner_id % topo_info.num_nodes;

    int cpu_id = topo_info.cpu_id_list[r][q];

    //printf("Affinity %d -> %d\n", runner_id, cpu_id);

    // This is for the SEAS machine
    //int cpu_id = runner_id;

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
