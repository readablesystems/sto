#pragma once

#ifndef MALLOC
#define MALLOC 2
#endif

#include <cpuid.h>
#include <cstddef>
#include <cstdint>
#include <cctype>
#include <cstring>

#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>

extern void allocator_init();
extern void set_affinity(int runner_id);
extern void discover_topology();

static constexpr uint32_t level_bstr  = 0x80000004;

enum class Reg : int {eax = 0, ebx, ecx, edx, size};

class CpuidQuery {
public:
    static constexpr uint32_t query_level = 0;
    static constexpr uint32_t result_bit = 0;
    static constexpr Reg result_reg = Reg::eax;
};

class TscQuery : public CpuidQuery {
public:
    static constexpr uint32_t query_level = 0x80000001;
    static constexpr uint32_t result_bit = (1 << 27);
    static constexpr Reg result_reg = Reg::edx;
};

class IvTscQuery : public CpuidQuery {
public:
    static constexpr uint32_t query_level = 0x80000007;
    static constexpr uint32_t result_bit = (1 << 8);
    static constexpr Reg result_reg = Reg::edx;
};

template <typename Query>
inline bool cpu_has_feature() {
    uint32_t max_level = __get_cpuid_max(0x80000000, nullptr);
    if (max_level < Query::query_level) {
        return false;
    } else {
        uint32_t regs[static_cast<int>(Reg::size)];
        int r = __get_cpuid(Query::query_level,
                            &regs[static_cast<int>(Reg::eax)],
                            &regs[static_cast<int>(Reg::ebx)],
                            &regs[static_cast<int>(Reg::ecx)],
                            &regs[static_cast<int>(Reg::edx)]);
        if (r == 0)
            return false;
        else
            return (regs[static_cast<int>(Query::result_reg)] & Query::result_bit);
    }
}

inline std::string get_cpu_brand_string() {
    uint32_t max_level = __get_cpuid_max(0x80000000, nullptr);
    if (max_level < level_bstr)
        return std::string(); // brand string not supported

    uint32_t regs[static_cast<int>(Reg::size)];
    std::string bs;
    for (unsigned int i = 0; i < 3; ++i) {
        int r = __get_cpuid(0x80000002 + i,
                            &regs[static_cast<int>(Reg::eax)],
                            &regs[static_cast<int>(Reg::ebx)],
                            &regs[static_cast<int>(Reg::ecx)],
                            &regs[static_cast<int>(Reg::edx)]);
        (void)r;
        for (int j = 0; j < static_cast<int>(Reg::size); ++j) {
            for (int k = 0; k < (int)sizeof(uint32_t); ++k) {
                auto c = reinterpret_cast<char *>(&regs[j])[k];
                if (c != 0x00)
                    bs.push_back(c);
            }
        }
    }
    return bs;
}

inline double get_cpu_brand_frequency() {
    static constexpr double multipliers[] = {0.001, 1.0, 1000.0};

    std::string bs = get_cpu_brand_string();
    std::cout << "Info: CPU detected as " << bs << std::endl;

    int unit = 0;
    std::string::size_type freq_str_end = bs.rfind("MHz");
    if (freq_str_end == std::string::npos) {
        ++unit;
        freq_str_end = bs.rfind("GHz");
    }
    if (freq_str_end == std::string::npos) {
        ++unit;
        freq_str_end = bs.find("THz");
    }
    if (freq_str_end == std::string::npos)
        return 0.0;

    auto freq_str_begin = freq_str_end;
    char c;
    do {
        --freq_str_begin;
        c = bs[freq_str_begin];
    } while (isdigit(c) || (c == '.'));
    ++freq_str_begin;
    auto len = freq_str_end - freq_str_begin;
    auto fs = bs.substr(freq_str_begin, len);
    std::stringstream ss(fs);
    double freq;
    ss >> freq;

    return freq * multipliers[unit];
}

inline double determine_cpu_freq() {
    allocator_init();
    discover_topology();
    double freq = 0.0;
    std::cout << "Checking for rdtscp support..." << std::flush;
    if (!cpu_has_feature<TscQuery>()) {
        std::cout << std::endl;
        std::cerr << "Fatal error: CPU lacks timestamp counter (tsc) capability." << std::endl;
        return freq;
    } else {
        std::cout << " Yes" << std::endl;
    }

    std::cout << "Checking for invariant tsc support..." << std::flush;
    if (!cpu_has_feature<IvTscQuery>()) {
        std::cout << std::endl;
        std::cout << "Warning: CPU does not report support for invariant tsc. Please double check timing measurement."
                  << std::endl;
    } else {
        std::cout << " Yes" << std::endl;
    }

    std::cout << "Determining processor frequency..." << std::endl;
    freq = get_cpu_brand_frequency();
    if (freq == 0.0) {
        std::cout << "Warning: Can't determine processor tsc frequency from CPU brand string. Using the default value "
                "of 1 GHz." << std::endl;
        freq = 1.0;
    }
    std::cout << "Info: CPU tsc frequency determined as "
              << std::fixed << std::setprecision(2) << freq << " GHz." << std::endl;
    return freq;
}

struct TopologyInfo {
    int num_nodes;
    std::vector<std::vector<int>> cpu_id_list;
};

extern TopologyInfo topo_info;
