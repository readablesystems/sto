#!/usr/bin/env python

import os, re, sys, json, subprocess, multiprocessing

bm_execs = ["../concurrent-1M", "./concurrent-50"]

# for use with boosting
# I compiled these with 022d56df086cbddc618a2feadc9ddbb9c3efc889's options.
bm_execs += ["../concurrent-sto", "../concurrent-boostingsto", "../concurrent-boostingstandalone"]

opacity_names = ["no opacity", "TL2 opacity", "slow opacity"]
scaling_txlens = [1, 4, 8, 128, 256, 512]
nthreads_max = multiprocessing.cpu_count()
nthreads_to_run_full = [1, 2, 4, 8, 16, 24]
nthreads_to_run_dual = [1, 24]

def attach_args(bm_idx, nthreads, txlen, opacity, ntrans, writepercent=None):
	args = [bm_execs[bm_idx], "3"]
	if opacity == 0:
		args.append("array-nonopaque")
	elif bm_idx > 1:
		args.append("hash")
	else:
		args.append("array")
	args.append("--ntrans=%d" % ntrans)
	args.append("--nthreads=%d" % nthreads)
	args.append("--opspertrans=%d" % txlen)
	if writepercent is not None:
		args.append("--writepercent=%s" % writepercent)
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
#	numtx = int(re.split(" ", re.search("(?<=, )[0-9]* transactions", output).group(0))[0])
#	tx_starts = int(re.split(" ", re.search("(?<=\$ )[0-9]* starts", output).group(0))[0])
#	tx_commits = int(re.split(" ", re.search("(?<=, )[0-9]* commits", output).group(0))[0])
#	tx_aborts = tx_starts - tx_commits
#	if tx_aborts > 0:
#		commit_aborts = int(re.split(" ", re.search("(?<=\$ )[0-9]* \(", output).group(0))[0])
#		abort_rate = float(tx_aborts) / float(tx_starts)
#		ct_abort_ratio = float(commit_aborts) / float(tx_aborts)
#	else:
#		commit_aborts = 0
#		abort_rate = 0.0
#		ct_abort_ratio = 0.0

	results = dict()
	results["time"] = time
#	results["num_txs"] = numtx
	results["array_size"] = size
#	results["tx_starts"] = tx_starts
#	results["tx_commits"] = tx_commits
#	results["tx_aborts"] = tx_aborts
#	results["commit_aborts"] = commit_aborts
#	results["abort_rate"] = abort_rate
#	results["commit_time_abort_ratio"] = ct_abort_ratio

	return results

def getRecordKey(bm_idx, trail, ntrans, nthreads, txlen, opacity, writepercent=None):
	ret = "%d/%d/%d/%d/%d/%d" % (bm_idx, trail, ntrans, nthreads, txlen, opacity)
	if writepercent:
		ret += "/%s" % writepercent
	return ret

def run_single(bm_idx, trail, txlen, opacity, records, nthreads, ntrans, writepercent=None):
	run_key = getRecordKey(bm_idx, trail, nthreads, txlen, opacity, ntrans, writepercent)
	args = attach_args(bm_idx, nthreads, txlen, opacity, ntrans, writepercent)
	print_cmd(args)
	bm_stdout = (to_strcmd(args) + "\n")

	single_out = subprocess.check_output(args, stderr=subprocess.STDOUT)
	records[run_key] = extract_numbers(single_out)
	bm_stdout += single_out
	return bm_stdout

def run_series(bm_idx, trail, txlen, opacity, records, nthreads_to_run, ntrans, writepercent=None):
	assert opacity >= 0 and opacity <= 2

	bm_stdout = "@@@ Running with %s, txlen %d. Trail #%d" % (opacity_names[opacity], txlen, trail)
	print bm_stdout
	bm_stdout += "\n"

	for nthreads in nthreads_to_run:
		bm_stdout += run_single(bm_idx, trail, txlen, opacity, records, nthreads, ntrans, writepercent)

	return bm_stdout

def save_results(exp_name, stdout, records):
	f = open(exp_name + "_stdout.txt", "w")
	f.write(stdout)
	f.close()

	f = open("experiment_data.json", "w")
	f.write(json.dumps(records, sort_keys=True, indent=2))
	f.close()

def exp_scalability_overhead(repetitions, records, opacity, txlens):
	print "@@@@\n@@@ Starting experiment: scalability-overhead:"
	ntxs = 8000000
	ttr = [1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24]
	combined_stdout = ""

	for trail in range(0, repetitions):
		for txlen in txlens:
			combined_stdout += run_series(0, trail, txlen, opacity, records, ttr, ntxs)

	save_results("scalability_overhead", combined_stdout, records)

def exp_scalability_hi_contention(repetitions, records):
	print "@@@@\n@@@ Starting experiment: scalability-hi-contention:"
	ntxs = 4000000
	ttr = nthreads_to_run_dual
	txlen = 25
	combined_stdout = ""

	for trail in range(0, repetitions):
		combined_stdout += run_series(1, trail, txlen, 0, records, ttr, ntxs)

	save_results("scalability_hi_contention", combined_stdout, records)

def exp_boosting_micro(repetitions, records):
	print "@@@@\n@@@ Starting experiment: boosting-microbenchmark:"
	ntxs = 10000000
	ttr = [16]
	writepercent = "0.1"
	txlen = 50
	combined_stdout = ""
	for trail in xrange(repetitions):
		for bm_idx in [2, 3, 4]:
			combined_stdout += run_series(bm_idx, trail, txlen, 1, records, ttr, ntxs, writepercent)

	save_results("boosting_micro", combined_stdout, records)
	#make concurrent BOOSTING_STO=1 && ./concurrent 3 hash --nthreads=16 --opspertrans=50 --ntrans=10000000 --writepercent=.1

def exp_scalability_largetx(repetitions, records):
	print "@@@@\n@@@ Starting experiment: scalability-largetx:"
	nitems = 7096320 # = 512 * 7 * 9 * 11 * 20 (for dividability)
	combined_stdout = ""

	for txlen in scaling_txlens:
		for trail in range(0, repetitions):
			combined_stdout += run_single(0, trail, txlen, 0, records, 1, nitems/txlen)

	for txlen in scaling_txlens:
		for trail in range(0, repetitions):
			combined_stdout += run_single(0, trail, txlen, 0, records, 16, nitems/txlen)	

	save_results("scalability_largetx", combined_stdout, records)

def exp_opacity_modes(repetitions, records):
	print "@@@@\n@@@ Starting experiment: opacity-modes:"
	ntxs = 8000000
	txlen = 50
	combined_stdout = ""

	for opacity in range(0, 3):
		for trail in range(0, repetitions):
			# low-contention
			combined_stdout += run_single(0, trail, txlen, opacity, records, 16, ntxs)
			# high-contention
			combined_stdout += run_single(1, trail, txlen/5, opacity, records, 16, ntxs/2)
	
	save_results("opacity_modes", combined_stdout, records)

def exp_opacity_tl2overhead(repetitions, records):
	print "@@@@\n@@@ Starting experiment: opacity_tl2overhead:"
	ntxs = [50000000, 5000000]
	ttr = nthreads_to_run_full
	txlens = [5, 50]
	combined_stdout = ""
	
	for opacity in range(0, 3):
		for tidx in range(0, len(txlens)):
			for trail in range(0, repetitions):
				combined_stdout += run_series(0, trail, txlens[tidx], opacity, records, ttr, ntxs[tidx])
	
	save_results("opacity_modes", combined_stdout, records)

def print_usage(script_name):
	usage = "Usage: " + script_name + """ num_rep
  num_rep: Integer number specifying the number of repeated runs for each experiment, 5 is a good choice"""
	print usage

def main(argc, argv):
	if argc < 2:
		print_usage(argv[0])
		sys.exit(0)

	repetitions = int(argv[1])
	if repetitions <= 0 or repetitions > 10:
		print "Please specify number of repetitions within integer range [1, 10]"
		sys.exit(0)

#	with open("experiment_data.json", "w+") as data_file:
#		records = json.load(data_file)
	records = dict()
	#exp_boosting_micro(repetitions, records)
	exp_scalability_overhead(repetitions, records, 0, [10, 50])
	exp_scalability_overhead(repetitions, records, 1, [10, 50])
	#exp_scalability_hi_contention(repetitions, records)
	#exp_scalability_largetx(repetitions, records)
	#exp_opacity_modes(repetitions, records)
	#exp_opacity_tl2overhead(repetitions, records)

if __name__ == "__main__":
	main(len(sys.argv), sys.argv)
