#!/usr/bin/python

import subprocess, shlex
from script_config import *
import time


CKP=False

def format_list(l):
    return str(l).replace(' ', '').replace('[', '').replace(']', '').replace('\'', '')

args = {
    "bench": lambda x: " --bench %s " % (x),
    "txnflags": lambda x: " --txn-flags 1 ",
    "runtime": lambda x: " --runtime %d " % (x),
    "verbose": lambda x: " --verbose " if x else "",
    "ckp_th": lambda x: " --ckp-th %d " % (x),
    "root_dir": lambda x: " --root-dir %s " % (x),
    "ckpdirs": lambda x: ''.join([" --ckp-dirs %s " % (format_list(i)) for i in x]),
    "scale_factor": lambda x: " --scale-factor %d " % (x),
    "workload": lambda x: " --bench-opts --workload-mix\ %s " % (x),
    "par": lambda x: " --parallel-loading " if x else "",
    "numa": lambda x: " --numa-memory %dG" %(x) if x > 0 else "",
    "num_th": lambda x: " --num-threads %d " % (x),
    "logfiles": lambda x: ''.join([" --logfile %s " % (i) for i in x]) if x != [] else "",
    "assignment": lambda x: ''.join([" --assignment %s " % (format_list(i)) for i in x]),
    "events": lambda x: " --stats-server-sockfile counters.sock " if x else "",
    "enable_par_ckp": lambda x: " --enable-par-ckp " if x else "",
    "ckp_compress": lambda x: " --ckp-compress  " if x else "",
    "reduced_ckp": lambda x: " --reduced-ckp " if x else "",
    "stats_file": lambda x: " --stats-file %s " % (x) if x else "",
    "stats_counters": lambda x: " --stats-counters %s " % (x) if x else "",
    "run_for_ckp": lambda x: "--run-for-ckp %s " % (x) if x else "",
    "recovery_test": lambda x: "--recovery-test" if x else "",
    }

def format_params(params):
    s = ""
    for k, v in params.iteritems():
        if k != "logfiles" and k != "assignment":
            s += args[k](params[k])
    # there is some required ordering of the arguments
    if "logfiles" in params:
        s += args["logfiles"](params["logfiles"])
    if "assignment" in params:
        s += args["assignment"](params["assignment"])
    return s

def gen_command(params, par, verbose, events):

    binary = ""
    if events:
        binary = "DISABLE_MADV_WILLNEED=1 ../out-perf.ectrs.masstree/benchmarks/dbtest --basedir tmp/ --stats-server-sockfile counters.sock "
    else:
        binary = "DISABLE_MADV_WILLNEED=1 ../out-perf.masstree/benchmarks/dbtest --basedir tmp/ "

    return binary \
        + args["par"](par) \
        + args["verbose"](verbose) \
        + args["events"](events) \
        + format_params(params)

def make(checkpoint, stripe=False, compress=False):
    opts = ""
    if checkpoint:
        opts += " CKP=1 "
    if stripe:
        opts += " PAR_CKP=1 "
    if compress:
        opts += " CKP_COMPRESS=1 "

    if EVENTS:
        opts += " EVENT_COUNTERS=1 "

    if PERF_TIME:
        opts += " PERF_TIME=1 "

    cmd = "cd ..\n"
    if CLEAN:
        cmd += "make clean\n"
    cmd += "make stats_client\nmake -j32 dbtest MODE=perf MASSTREE=1 PROTO2=1  "
    cmd += opts
    cmd += "\ncd benchmarks"

    print "Running: ", cmd

    p = subprocess.Popen(cmd, shell=True)
    p.wait()

def generate(par_load, params):

    par = ""
    if par_load:
        par = "--parallel-loading"

    cmd = "rm /f*/" + USER + "/*; rm /f*/" + USER + "/disk*/*; rm ./disk/*; rm /data/"+USER+"/*; rm ~/disk/*; rm /data/"+USER+"/disk*/*; \n"
    cmd += """echo "%s" >> output \n""" % (params["bench"])
    cmd += """echo "WORKLOAD %s" >> output \n""" % (params["workload"])

    cmd += gen_command(params, par, VERBOSE, EVENTS)
    cmd += " >> output 2>> output2\n"

    print "Running: ", cmd

    # write the cmd into the params["stats_file"]
    f = open(params["stats_file"], 'w')
    f.write(cmd)
    f.close()

    # TODO: should remember to pipe stdout/stderr to the right files

    p = subprocess.Popen(cmd, shell=True)
    p.wait()

make(True)
for x in xrange(1):
    for c in CONFIG:
        c["stats_file"] = gen_stats_file()
        generate(True, c)
