#set up matplotlib and the figure
import matplotlib.pyplot as plt
from collections import defaultdict


BENCHMARK_FILE = "cds_benchmarks_stats.txt"
INIT_SIZES = [1000, 10000, 50000, 100000, 150000]
NTHREADS = [1,2,4,8]#4, 8, 12, 16, 20]
TESTS = ["PQRandSingleOps:R","PQRandSingleOps:D","PQPushPop:R",
            "PQPushPop:D", "PQPushOnly:R", "PQPushOnly:D", 
            "Q:PushPop","Q:RandSingleOps",
            "HM1M:F34,I33,E33","HM1M:F90,I5,E5",
            "HM1MMultiOp:F34,I33,E33","HM1MMultiOp:F90,I5,E5",
            "HM125K:F34,I33,E33","HM125K:F90,I5,E5",
            "HM10K:F34,I33,E33","HM10K:F90,I5,E5",
        ]
QUEUES = [
    "Queue1", "Queue2", "FCQueue2", "FCQueueLP1", "FCQueueLP2", 
    "Wrapped FCQueueNT1", "Wrapped FCQueueNT2", "Wrapped CDSFCQueue",
    "FCQueueNT1", "FCQueueNT2", "CDSFCQueue"]
'''
Parse the data file cds_benchmarks_stats.txt
We should end up with a dictionary with the following format:
    
    test_name {
        size1: [
            [data structure data for 1 threads],
            [data structure data for 2 threads],
            ...
        ]
        size2: [
            [data structure data for 1 threads],
            [data structure data for 2 threads],
            ...
        ]
    }
''' 

tests = defaultdict(dict)
size_index = 0
test = ""
with open(BENCHMARK_FILE, "r") as f:
    for line in f.read().splitlines():
        if (line == ""):
            size_index += 1
            size_index %= len(INIT_SIZES)
        elif line in TESTS:
            test = line    
            size_index = 0
        else:
            if INIT_SIZES[size_index] not in tests[test]:
                tests[test][INIT_SIZES[size_index]] = []
            tests[test][INIT_SIZES[size_index]].append(
                    [float(x) for x in line.split(",")[:-1]])

for test, results in tests.items():
    for size, data_lists in results.items():
        if len(data_lists[0]) == 0:
            continue
        plt.figure
        #plot data
        qdata = defaultdict(list)
        for q_index in range(len(data_lists[0])):
            for i in range(len(data_lists)):
                qdata[q_index].append(data_lists[i][q_index])
        for i in range(len(data_lists[0])):
            plt.plot(NTHREADS, qdata[i], label=QUEUES[i])

        #add in labels and title
        plt.xlabel("Number of Threads")
        plt.ylabel("Speed (ops/s)")
        plt.title("Queue Performance")

        #add limits to the x and y axis
        plt.xlim(0, 24)
        plt.ylim(-5, 20000000) 

        #create legend
        plt.legend(loc="upper left")

        #save figure to png
        plt.show()
        plt.savefig("QueueSize%d.png" % size)
