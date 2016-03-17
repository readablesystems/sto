#!/usr/bin/python

import os
import sys
import subprocess
import re

try:
    FILENAME = sys.argv[1]
except:
    print sys.argv[0], 'outputfile'
    sys.exit(0)
try:
    if sys.argv[1] == '-h' or sys.argv[1] == '--help':
        print sys.argv[0], 'outputfile'
        sys.exit(0)
except: pass

CMD="./pqVsIt "
MANY_THREADS = lambda n: "--nthreads=%d" % n


OUTPUT_DIR="./"
ROUNDS = 5

def basic_test(threads):
    cmd = CMD

    cmd += threads
    
    pq_pts = []
    it_pts = []
    for i in xrange(ROUNDS):
        cmd1 = cmd + " --seed=" + str(i)
        out = subprocess.check_output(cmd1, shell=True)
        pq = re.search("(?<=PQ: )[0-9]*", out).group(0);
        it = re.search("(?<=it: )[0-9]*", out).group(0);
        pq_pts.append(float(pq))
        it_pts.append(float(it))
    s = "PQ\n"
    s = s + repr(pq_pts) + "\n"
    s = s + "It\n"
    s = s + repr(it_pts) + "\n"
    return s

def stdtest(f, threads, testname):
    f.write(testname + '\n')
    f.write("%s\n" % (basic_test(threads)))
    
def run():
    f = open(OUTPUT_DIR + '/' + FILENAME, 'w')

    for i in [1,2,4,8,16,24]:
        stdtest(f, MANY_THREADS(i), "%d threads" % i)
    f.close()

run()
