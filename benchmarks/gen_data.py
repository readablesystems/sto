#!/usr/bin/python

# generates a set of data for plotting the throughput over time

import sys
import re

f = open(sys.argv[1])

bench = re.compile("ycsb|tpcc$")
r = re.compile("([0-9]*)\.[0-9]*s: \[(\d+), (\d+)\] (\d+)$")

current_bench = None

data = {}
prev = {}

for line in f:
    #print line
    m = bench.match(line)
    if m:
        current_bench = line
        print line
    else:
        m = r.match(line)
        if m:
            time = int(m.group(1)) * 1.0 / 10**6
            commits = int(m.group(2))
            aborts = int(m.group(3))
            thread = int(m.group(4))

            if thread not in data:
                data[thread] = []
                prev[thread] = (time, commits, aborts)
            else:
                (prev_t, prev_c, prev_a) = prev[thread]
                data[thread].append((time, (commits - prev_c) / (time - prev_t)))
                prev[thread] = (time, commits, aborts)

f.close()

f = open("./data", 'w')

f.write("data=")
f.write(str(data))
f.close()
