#include "SystemProfiler.hh"

namespace Profiler {

pid_t spawn(const std::string &name, perf_mode mode) {
    std::stringstream ss;
    ss << getpid();

    auto profile_name = name + ((mode == perf_mode::record) ? ".data" : ".txt");

    pid_t pid = fork();
    if (pid == 0) {
        int console = open("/dev/null", O_RDWR);
        always_assert(console > 0);
        always_assert(dup2(console, STDOUT_FILENO) != -1);
        always_assert(dup2(console, STDERR_FILENO) != -1);
        if (mode == perf_mode::record) {
            exit(execl("/usr/bin/perf", "perf", "record", "-g", "-o", profile_name.c_str(),
                       "-p", ss.str().c_str(), nullptr));
        } else if (mode == perf_mode::counters) {
            exit(execl("/usr/bin/perf", "perf", "stat", "-e",
                       "cycles,cache-misses,cache-references,L1-dcache-loads,L1-dcache-load-misses,"
                       "context-switches,cpu-migrations,page-faults,branch-instructions,branch-misses,"
                       "dTLB-load-misses,dTLB-loads",
                       "-o", profile_name.c_str(),
                       "-p", ss.str().c_str(), nullptr));
        }
    }

    return pid;
}

bool stop(pid_t pid) {
    if (kill(pid, SIGINT) != 0)
        return false;
    if (waitpid(pid, nullptr, 0) != pid)
        return false;
    return true;
}

void profile(const std::string &name, std::function<void(void)> profilee) {
    pid_t pid = spawn(name, perf_mode::record);

    profilee();

    bool ok = stop(pid);
    always_assert(ok);
}

void profile(std::function<void(void)> profilee) {
    profile("perf", profilee);
}

}
