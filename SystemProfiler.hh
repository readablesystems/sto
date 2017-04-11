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
        exit(execl("/usr/bin/perf", "perf", "record", "-g", "-o", profile_name.c_str(), "-p", ss.str().c_str(), nullptr));
    }

    profilee();

    kill(pid, SIGINT);
    waitpid(pid, nullptr, 0);
}

void profile(std::function<void(void)> profilee) {
    profile("perf", profilee);
}

};
