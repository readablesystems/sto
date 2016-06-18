#!/bin/bash

cd .. && make cds_benchmarks && ./cds_benchmarks
cd microbenchmarks/
python cds_benchmarks.py
