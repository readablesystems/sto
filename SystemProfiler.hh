#pragma once

#include <sstream>
#include <iostream>
#include <functional>
#include <string>
#include <cassert>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

namespace Profiler {

void profile(const std::string& name, std::function<void(void)> profilee) {
    std::stringstream ss;
    ss << getpid();

    auto profile_name = name + ".data";

    pid_t pid = fork();
    if (pid == 0) {
        int console = open("/dev/null", O_RDWR);
        assert(console > 0);
        assert(dup2(console, STDOUT_FILENO) != -1);
        assert(dup2(console, STDERR_FILENO) != -1);
        exit(execl("/usr/bin/perf", "perf", "record", "-o", profile_name.c_str(), "-p", ss.str().c_str(), nullptr));
    }

    profilee();

    kill(pid, SIGINT);
    waitpid(pid, nullptr, 0);
}

void profile(std::function<void(void)> profilee) {
    profile("perf", profilee);
}

// Profiling counters measuring run time breakdowns
enum TimingCounters {
    tc_commit = 0,
    tc_validate,
    tc_find_item,
    tc_abort,
    tc_cleanup,
    tc_opacity,
    tc_count
};

typedef uint64_t tc_counter_type;

template <unsigned C, unsigned N, bool Less = (C < N)>
struct tc_helper;

template <unsigned C, unsigned N>
struct tc_helper<C, N, true> {
    static bool counter_exists(unsigned tc) {
        return tc < N;
    }
    static void account_array(tc_counter_type *tcs, tc_counter_type v) {
        tcs[C] += v;
    }
};

template <unsigned C, unsigned N>
struct tc_helper<C, N, false> {
    static bool counter_exists(unsigned) { return false; }
    static void account_array(tc_counter_type*, tc_counter_type) {}
};

struct tc_counters {
    tc_counter_type tcs_[tc_count];
    tc_counters() { reset(); }
    tc_counter_type timing_counter(int name) {
        return tc_helper<0, tc_count>::counter_exists(name) ? tcs_[name] : 0;
    }
    void reset() {
        for (int i = 0; i < tc_count; ++i)
            tcs_[i] = 0;
    }
};

};
