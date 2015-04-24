#!/usr/bin/python

# generates a set of data for plotting the throughput over time

import matplotlib
matplotlib.use('Agg')

import sys
import re
import numpy

import matplotlib.pylab as plt

f = open(sys.argv[1])

linereg = re.compile("(.*) (\d+) (\d+) (\d+) (\d+)")

current_bench = None

data = {}
time = {}

counter = 0
commit_counter = 0
counter_inst_latency = 0
sample_rate = 300
checkpoint_periods = []
in_ckp = False

def movingaverage(interval, window_size):
    window = numpy.ones(int(window_size))/float(window_size)
    return numpy.convolve(interval, window, 'same')

for line in f:
    l = line.split(' ')
    if not linereg.match(line):
        continue
    var_name = l[0]
    time_stamp = int(l[1])
    val_1 = int(l[2])
    val_2 = int(l[3])

    if var_name not in data:
        data[var_name] = []
        time[var_name] = time_stamp

    diff = time_stamp - time[var_name]
    if val_2 is not 0:
	if var_name == 'persist_latency':
            # persist latency
            data['persist_latency'].append((diff, val_2 * 1.0 / val_1 / 1e3))
        else:
            data[var_name].append((diff, val_2 * 1.0 / val_1))

    else:
        if var_name == 'persist_throughput':
            # persist throughput
            if diff > 0 and counter % sample_rate == 0:
                prev = data['persist_throughput'][-1] if data['persist_throughput'] != [] else (0, 0, 0)
                if (val_1 - prev[2] > 0):
                    data['persist_throughput'].append((diff, (val_1 - prev[2]) * 1.0 / ((diff - prev[0]) *1.0 / 1e6), val_1))
            counter += 1
        elif var_name == 'commit_throughput':
            # persist throughput
            if diff > 0 and commit_counter % sample_rate == 0:
                prev = data['commit_throughput'][-1] if data['commit_throughput'] != [] else (0, 0, 0)
                if (val_1 - prev[2] > 0):
                    data['commit_throughput'].append((diff, (val_1 - prev[2]) * 1.0 / ((diff - prev[0]) *1.0 / 1e6), val_1))
            commit_counter += 1
        elif var_name == "inst_latency":
            counter_inst_latency += 1
            prev = data['inst_latency'][-1] if data['inst_latency'] != [] else (0, 0, 0)
            if counter_inst_latency % sample_rate == 0:
                data['inst_latency'].append((diff, val_1 * 1.0 / 1e3, val_1))

        else:
            if val_1 > 500000 and var_name == "inst_fsync_time":
                print time_stamp, val_1, diff*1.0/1e6
            data[var_name].append((diff, val_1))

    if var_name == "checkpoint_period":
        if not in_ckp and val_1 == 1:
            in_ckp = True
            checkpoint_periods.append((diff/1e6, 10000))
        if in_ckp and val_1 == 0:
            in_ckp = False
            (low, high) = checkpoint_periods[-1]
            checkpoint_periods[-1] = ((low, diff/1e6))

f.close()

offset=0
offset_end=0
x_cutoff = 400

for k, v in data.iteritems():
    fig, ax1 = plt.subplots()
    #plt.plot([x[0]/1e6 for x in v][offset:], [x[1] for x in v][offset:])
    x = [i[0]/1e6 for i in v][offset:]
    y = [i[1] for i in v][offset:]

    if k == "persist_throughput":
        y_av = movingaverage(y, 5)
        plt.plot(x, y)
        plt.xlabel("Time (seconds)")
        plt.ylabel("Throughput (txns/s)")
    elif k == "persist_latency":
        plt.plot(x, y)
        plt.xlabel("Time (seconds)")
        plt.ylabel("Latency rolling average (ms)")
    elif k == "logger_total_bytes_fsync":
        plt.plot(x, y)
        plt.xlabel("Time (seconds)")
        plt.ylabel("Bytes")
    elif k == "avg_logger_fsync_time":
        plt.plot(x, y)
        plt.xlabel("Time (seconds)")
        plt.ylabel("Time (us)")
    elif k == "inst_latency":
        y_av = movingaverage(y, 5)
        plt.plot(x, y_av)
        plt.xlabel("Time (seconds)")
        plt.ylabel("Time (us)")
    else:
        plt.plot(x, y)

    x1, x2, y1, y2 = plt.axis()
    plt.axis((0, x2, 0, y2))

    if checkpoint_periods is not []:
        for ckp in checkpoint_periods:
            ax1.axvspan(ckp[0], ckp[1], facecolor='gray', alpha=0.5)
    plt.savefig("fig" + k + ".pdf")
    plt.close()


