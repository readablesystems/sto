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

static constexpr auto perf_path = "/usr/bin/perf";

void profile(const std::string& name, std::function<void(void)> profilee) {
    std::stringstream ss;
    ss << getpid();

    auto profile_name = name + ".data";

    pid_t pid = fork();
    if (pid == 0) {
        int console = open("/dev/null", O_RDWR);
        int rc;
        (void)rc;
        assert(console > 0);
        rc = dup2(console, STDOUT_FILENO);
        assert(rc != -1);
        rc = dup2(console, STDERR_FILENO);
        assert(rc != -1);
        exit(execl(perf_path, "perf", "record", "-o", profile_name.c_str(), "-p", ss.str().c_str(), nullptr));
    }

    profilee();

    kill(pid, SIGINT);
    waitpid(pid, nullptr, 0);
}

void profile(std::function<void(void)> profilee) {
    profile("perf", profilee);
}

class PerfProfiler {
public:
    PerfProfiler() : running(false), child_pid(0) {}

    void start() {
        if (running) {
            std::cerr << "Profiler already running" << std::endl;
            return;
        }

        std::stringstream spid;
        spid << getpid();

        pid_t pid = fork();
        if (pid < 0) {
            std::cerr << "Error: cannot start profiler" << std::endl;
            return;
        }
        if (pid == 0) {
            int redir = open("/dev/null", O_RDWR);
            assert(redir > 0);
            int rc;
            rc = dup2(redir, STDOUT_FILENO);
            assert(rc != -1);
            rc = dup2(redir, STDERR_FILENO);
            assert(rc != -1);
            rc = execl(perf_path, "perf", "record", "-o", "perf.data", "-p", spid.str().c_str(), nullptr);
            exit(rc);
        } else {
            running = true;
            child_pid = pid;
        }
    }

    void stop() {
        if (!running) {
            std::cerr << "Profiler not running" << std::endl;
            return;
        }

        kill(child_pid, SIGINT);
        waitpid(child_pid, nullptr, 0);

        running = false;
        child_pid = 0;
    }

private:
    bool running;
    pid_t child_pid;
};

}
