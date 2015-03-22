#!/usr/bin/env python

import os, re, sys, json, numpy, subprocess, multiprocessing

bm_dir = "../"
bm_prog = "concurrent"
bm_exec = bm_dir + bm_prog

opacity_names = ["no opacity", "TL2 opacity", "slow opacity"]
scaling_txlens = [5, 10, 25, 50, 75, 150, 200, 250, 300]
fixed_txlen = 100
nthreads_max = multiprocessing.cpu_count()

def clean_up():
	print "Cleanning up sto directory..."
	ret = os.system("cd %s && make clean > /dev/null 2>&1" % bm_dir)
	assert ret == 0
	print "Done."

def build():
	if os.path.exists(bm_exec):
		clean_up();

	assert not os.path.exists(bm_exec)

	print "Building benchmark..."

	ret = os.system("cd %s && make -j%d > /dev/null 2>&1" % (bm_dir, nthreads_max))
	if ret != 0:
		print "FATAL: Build error!"
		sys.exit(1)
	else:
		print "Built."

def attach_args(nthreads, txlen, opacity):
	args = [bm_exec, "3", "array", "--ntrans=4000000"]
	args.append("--nthreads=%d" % nthreads)
	args.append("--opspertrans=%d" % txlen)
	args.append("--opacity=%d" % opacity)
	return args

def to_strcmd(args):
	cmd = ""
	for a in args:
		cmd += (a + " ")
	return cmd[:-1]

def print_cmd(args):
	print to_strcmd(args)


def extract_numbers(output):
	time = float(re.search("(?<=real time: )[0-9]*\.[0-9]*", output).group(0))
	size = int(re.search("(?<=ARRAY_SZ: )[0-9]*", output).group(0))
	numtx = int(re.split(" ", re.search("(?<=, )[0-9]* transactions", output).group(0))[0])
	tx_starts = int(re.split(" ", re.search("(?<=\$ )[0-9]* starts", output).group(0))[0])
	tx_commits = int(re.split(" ", re.search("(?<=, )[0-9]* commits", output).group(0))[0])
	tx_aborts = tx_starts - tx_commits
	if tx_aborts > 0:
		commit_aborts = int(re.split(" ", re.search("(?<=\$ )[0-9]* \(", output).group(0))[0])
		abort_rate = float(tx_aborts) / float(tx_starts)
		ct_abort_ratio = float(commit_aborts) / float(tx_aborts)
	else:
		commit_aborts = 0
		abort_rate = 0.0
		ct_abort_ratio = 0.0

	results = dict()
	results["time"] = time
	results["num_txs"] = numtx
	results["array_size"] = size
	results["tx_starts"] = tx_starts
	results["tx_commits"] = tx_commits
	results["tx_aborts"] = tx_aborts
	results["commit_aborts"] = commit_aborts
	results["abort_rate"] = abort_rate
	results["commit_time_abort_ratio"] = ct_abort_ratio

	return results

def getRecordKey(trail, nthreads, txlen, opacity):
	return "%d/%d/%d/%d" % (trail, nthreads, txlen, opacity)

def run_series(trail, txlen, opacity, records, start_nthreads):
	assert opacity >= 0 and opacity <= 2

	nthreads = start_nthreads

	bm_stdout = "@@@ Running with %s, txlen %d. Trail #%d" % (opacity_names[opacity], txlen, trail)
	print bm_stdout
	bm_stdout += "\n"

	while nthreads < nthreads_max:
		run_key = getRecordKey(trail, nthreads, txlen, opacity)
		args = attach_args(nthreads, txlen, opacity)
		print_cmd(args)
		bm_stdout += (to_strcmd(args) + "\n")

		single_out = subprocess.check_output(args, stderr=subprocess.STDOUT)
		bm_stdout += single_out
		records[run_key] = extract_numbers(single_out)

		nthreads = nthreads * 2

	return bm_stdout

def run_fix_txlen(repetitions, records, txlen):
	print "@@@ Tx length fixed to %d, repeat each experiment %d for trails" % (txlen, repetitions)
	combined_stdout = ""
	for opacity in range(0,3):
		for trail in range(0, repetitions):
			combined_stdout += run_series(trail, txlen, opacity, records, 1)

	f = open("fixed_txlen_stdout.txt", "w")
	f.write(combined_stdout)
	f.close()

def run_scale_txlen(repetitions, records):
	print "@@@ Scaling Tx length, opacity fixed to %s\n\
	Repeating each experiment for %d trails" % (opacity_names[1], repetitions)
	combined_stdout = ""
	for txlen in scaling_txlens:
		for trail in range(0, repetitions):
			combined_stdout += run_series(trail, txlen, 1, records, 16)

	f = open("scale_txlen_stdout.txt", "w")
	f.write(combined_stdout)
	f.close()

def print_usage(script_name):
	usage = "Usage: " + script_name + """ num_rep [mode]
  num_rep: Integer number specifying the number of repeated runs for each experiment, should be within range [1,10]
  mode: Optional argument specifying which benchmark(s) to execute, only takes integer numbers 1, 2, or 3:
    1 - Opacity microbenchmark: Fixed transaction length (%d) with all three opacity implementations
    2 - Transaction length microbenchmark: Scale transaction length with the TL2 opacity implementation
    3 - Run both benchmarks above (default)""" % fixed_txlen
	print usage

def main(argc, argv):
	if argc < 2:
		print_usage(argv[0])
		sys.exit(0)

	repetitions = int(argv[1])
	if repetitions <= 0 or repetitions > 10:
		print "Please specify number of repetitions within integer range [1, 10]"
		sys.exit(0)

	mode = 3
	if argc == 3:
		mode = int(argv[2])
	if mode <= 0 or mode > 3:
		mode = 3

	build()

	records = dict()

	f = open("experiment_data.json", "w")

	if mode & 0x1 != 0:
		run_fix_txlen(repetitions, records, fixed_txlen)
		f.write(json.dumps(records))
	if mode & 0x2 != 0:
		run_scale_txlen(repetitions, records)
		f.write(json.dumps(records))

	f.close()

if __name__ == "__main__":
	main(len(sys.argv), sys.argv)
