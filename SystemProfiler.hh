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

pid_t spawn(const std::string& name) {
    std::stringstream ss;
    ss << getpid();

    auto profile_name = name + ".data";

    pid_t pid = fork();
    if (pid == 0) {
        int console = open("dev/null", O_RDWR);
        always_assert(console > 0);
        always_assert(dup2(console, STDOUT_FILENO) != -1);
        always_assert(dup2(console, STDERR_FILENO) != -1);
        exit(execl("/usr/bin/perf", "perf", "record", "-g", "-o", profile_name.c_str(),
            "-p", ss.str().c_str(), nullptr));
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

void profile(const std::string& name, std::function<void(void)> profilee) {
    pid_t pid = spawn(name);

    profilee();

    bool ok = stop(pid);
    always_assert(ok);
}

void profile(std::function<void(void)> profilee) {
    profile("perf", profilee);
}

};
