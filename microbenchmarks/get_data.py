#!/usr/bin/env python

import sys, json, numpy, run_benchmarks

records_filename = "experiment_data.json"

def parse_exp_data_file():
	with open(records_filename) as data_file:
		return json.load(data_file)

def print_table_fixed_txlen(data):
	opacity_modes = data.keys()
	num_threads = sorted(data[opacity_modes[0]].keys())

	out = "\\begin{tabular}{ |c|"
	out += "c|" * len(opacity_modes)
	out += " }\n\\hline\n"

	out += "number of threads"
	for on in opacity_modes:
		out += (" & %s" % on)
	out += "\\\\\\hline\n"

	for ntr in num_threads:
		line = "%d" % ntr
		for on in opacity_modes:
			line += (" & %f" % data[on][ntr]["med_time"])
		line += "\\\\\\hline\n"
		out += line

	out += "\\end{tabular}\n"
	return out

def print_table_variable_txlen(data):
	txlens = sorted(data.keys())

	out = "\\begin{tabular}{ |c|c| }\n\\hline\n"
	
	for txlen in txlens:
		out += "%d & %f \\\\\\hline\n" % (txlen, data[txlen]["med_time"])

	out += "\\end{tabular}\n"
	return out

# experiments where we tested 3 opacity modes and fixed txlen to 100
def calc_results_opacity100(ntrails, records):
	data = calc_results("opacity100", ntrails, records)
	print "exp=opacity100, txlen=100:"
	out = print_table_fixed_txlen(data)
	print out

# experiments where we compare the overhead to TL2 style opacity, txlen
# was fixed to 25
def calc_results_tl2overhead(ntrails, records):
	data = calc_results("tl2overhead", ntrails, records)
	print "exp=tl2overhead, txlen=25:"
	out = print_table_fixed_txlen(data)
	print out

# experiments where we scale the sizes of transactions with nthreads=16
# and opacity=1 (TL2)
def calc_results_scaletxlen(ntrails, records):
	data = calc_results("scaletxlen", ntrails, records)
	print "exp=scale_txlen, nthreads=16, opacity=tl2:"
	out = print_table_variable_txlen(data)
	print out

# general framework for time extraction
def calc_results(exp_name, ntrails, records):
	processed_results = dict()
	tempdata = []

	if exp_name == "opacity100":
		opacity_top = 3
	elif exp_name == "tl2overhead":
		opacity_top = 2

	if exp_name == "opacity100" or exp_name == "tl2overhead":

		for opacity in range(0, opacity_top):
			on = run_benchmarks.opacity_names[opacity]
			processed_results[on] = dict()
			for nthreads in run_benchmarks.nthreads_to_run:
				processed_results[on][nthreads] = dict()
				tempdata = []
				for trail in range(0, ntrails):
					if exp_name == "opacity100":
						r = records[run_benchmarks.getRecordKey(trail, nthreads, 100, opacity)]["time"]
					elif exp_name == "tl2overhead":
						r = records[run_benchmarks.getRecordKey(trail, nthreads, 25, opacity)]["time"]
					tempdata.append(r)
				processed_results[on][nthreads]["med_time"] = numpy.median(tempdata)
				processed_results[on][nthreads]["stddev"] = numpy.std(tempdata)

	elif exp_name == "scaletxlen":

		nthreads = 16
		txlens = run_benchmarks.scaling_txlens
		txlens.append(run_benchmarks.s_fixed_txlen)
		txlens.append(run_benchmarks.fixed_txlen)
		txlens = sorted(txlens)

		for txlen in run_benchmarks.scaling_txlens:
			processed_results[txlen] = dict()
			tempdata = []
			for trail in range(0, ntrails):
				r = records[run_benchmarks.getRecordKey(trail, nthreads, txlen, 1)]
				tempdata.append(r)
			processed_results[txlen]["med_time"] = numpy.median(tempdata)
			processed_results[txlen]["stddev"] = numpy.std(tempdata)
	
	f = open("processed_%s.json" % exp_name, "w")
	f.write(json.dumps(processed_results, sort_keys=True, indent=2))
	f.close()

	return processed_results

def main(argc, argv):
	if argc != 2:
		print_usage(argv[0])
		sys.exit(0)
	
	ntrails = int(argv[1])
	if ntrails <= 0 or ntrails >10:
		sys.exit(0)
	
	records = parse_exp_data_file()
	
	calc_results_opacity100(ntrails, records)
	calc_results_tl2overhead(ntrails, records)
	calc_results_scaletxlen(ntrails, records)

if __name__ == "__main__":
	main(len(sys.argv), sys.argv)
