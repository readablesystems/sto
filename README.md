[![Build Status](https://travis-ci.org/readablesystems/sto.svg?branch=master)](https://travis-ci.org/readablesystems/sto)

# STO: Software Transactional Objects

STO (/stō/, pronounced the same as "stow") is a software transactional
memory (STM) library and experimental
platform written in C++. STO distinguishes itself from other STM
libraries in that it uses data type information derived from the
programming language, thus drastically reducing footprint of
transactions and false conflicts when compared to an untyped STM system.
Please check out our [EuroSys '16
paper](http://www.read.seas.harvard.edu/~kohler/pubs/herman16type-aware.pdf)
for more information.

STO was created by Nathaniel Herman as a Harvard undergrad.

## Installation

We tested our build on Linux (Ubuntu 16.04 LTS and later) only. Building on other platforms
should technically be possible, because we use standard C++/POSIX calls and avoid
hacks as much as we can.
There is a
[known issue](https://stackoverflow.com/questions/16596876/object-file-has-too-many-sections)
with Win32 object file format, so building under Windows (Cygwin or the Windows Subsystem for
Linux in Windows 10) is not recommended.

### Dependencies

- Modern C++ compiler with C++14 support
  - If you use GNU C Compiler (`g++`), version 7.2+ is recommended.
- GNU build system (`autoreconf` and `make` in particular)
- `cmake` 3.8+ (Optional)
- jemalloc
- libnuma
- `ninja` build system
- masstree and third-party libraries (as git submodules)

Please refer to your own system documentation on how to install these
dependencies prior to building STO. On Ubuntu 16.04 LTS or later, you
can install all dependencies by using:
```bash
$ sudo apt update
$ sudo apt install build-essential cmake libjemalloc-dev libnuma-dev ninja-build
```
If you wish to install `g++` version 7 on Ubuntu 16.04 or older systems, you can
use the following PPA package:
```bash
$ sudo add-apt-repository ppa:ubuntu-toolchain-r/test
$ sudo apt update
$ sudo apt install g++-7
```

### Build

1. Clone the git repository
```bash
$ git clone https://github.com/readablesystems/sto.git
$ cd sto
```

2. Initialize submodules
```bash
$ git submodule update --init --recursive
```

3. Execute configuration scripts
```bash
$ ./bootstrap.sh
$ ./configure
```
The `configure` script lets you specify the compiler to use when building STO.
For example, if `g++-7` is not the default compiler in your system, you can
enable it for STO by running `./configure CC=gcc-7 CXX=g++-7`.

(Note: if you use macOS you should probably run `./configure CXX='clang++ -stdlib=libc++'`)

4. Build
```bash
$ make -jN # launch N parallel build jobs
```
This builds all targets, which include all tests and benchmarks. If you
don't want all of those, you can build selected targets as well.

Here are some targets you may find useful:

- `make check`: Build and run all unit tests. This is the target used
by continuous integration.
- `make tpcc_bench`: Build the TPC-C benchmark.
- `make ycsb_bench`: Build the YCSB-like benchmark.
- `make micro_bench`: Build the array-based microbenchmark.
- `make clean`: You know what it does.

See [Wiki](https://github.com/readablesystems/sto/wiki) for advanced buid options.

## IDE Support & cmake

STO contains `cmake` configuration files that can be used by IDEs like
[JetBrains CLion](https://www.jetbrains.com/clion/). You can potentially
also use `cmake` to configure and build STO, but it is not recommended.

Full support for `cmake` will come soon.

## Develop New Data Types

You can implement your own data type in STO to extend the transactional
data type library. Please see the [Wiki](https://github.com/readablesystems/sto/wiki) on how to implement a STO
data type.

You can also take a look at `datatype/TBox.hh` for a simple example.

## Bug Reporting

You can report bugs by opening issues or contacting [Yihe
Huang](https://github.com/huangyihe).
