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
#include "compiler.hh"

namespace Profiler {

enum class perf_mode : int { record = 1, counters };

pid_t spawn(const std::string& name, perf_mode mode);

bool stop(pid_t pid);

void profile(const std::string& name, std::function<void(void)> profilee);

void profile(std::function<void(void)> profilee);

};
