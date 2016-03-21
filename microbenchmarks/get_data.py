#!/usr/bin/env python

import sys, json, numpy, run_benchmarks

records_filename = "experiment_data.json"
config_filename = "exp_config.json"

ntrails = 5

def parse_exp_data_file():
	with open(records_filename) as data_file:
		return json.load(data_file)

def parse_exp_config_file():
	with open(config_filename) as data_file:
		return json.load(data_file)

def print_csv(rows):
	csv_str = ""
	for row in rows:
		row_str = ""
		for cell in row:
			row_str += (cell + ",")
		row_str += "\n"
		csv_str += row_str
	print csv_str

def print_scalability_overhead(records, config):
	name = "scalability_overhead"
	f_on = run_benchmarks.opacity_names[config[name]["opacity"][0]]
	tls = config[name]["txlen"]
	ttr = config[name]["ttr"]
	rows = []
	curr_row = ["number of cores", "speedup"]
	rows.append(curr_row)

	data = process_results(name, config[name], records, ["time"])

	for f_tl in tls:
		baseline = data[f_on][f_tl][1]["med_time"]
		for tr in ttr:
			speedup = baseline / data[f_on][f_tl][tr]["med_time"]
			rows.append(["%d" % tr, "%.4f" % speedup])

	f_on = run_benchmarks.opacity_names[config[name]["opacity"][1]]
	#f_tl = 10
	for f_tl in tls:
		baseline = data[f_on][f_tl][1]["med_time"]
		for tr in ttr:
			baseline = data[f_on][f_tl][1]["med_time"]
			speedup = baseline / data[f_on][f_tl][tr]["med_time"]
			rows.append(["%d" % tr, "%.4f" % speedup])

	print "Experiment: %s\n" % name
	print_csv(rows)
	print "\n\n"

def print_scalability_hi_contention(records, config):
	name = "scalability_hi_contention"
	f_on = run_benchmarks.opacity_names[config[name]["opacity"][0]]
	f_tl = config[name]["txlen"][0]
	ttr = config[name]["ttr"]
	rows = []
	rows.append(["number of cores", "speedup"])

	data = process_results(name, config[name], records, ["time", "abort_rate"])

	baseline = data[f_on][f_tl][1]["med_time"]
	
	for tr in ttr:
		speedup = baseline / data[f_on][f_tl][tr]["med_time"]
		rows.append(["%d" % tr, "%.4f" % speedup])

	print "Experiment: %s\n" % name
	print_csv(rows)
	print "\n\n"

def print_scalability_largetx(records, config):
	name = "scalability_largetx"
	f_on = run_benchmarks.opacity_names[config[name]["opacity"][0]]
	trs = config[name]["ttr"]
	txlens = config[name]["txlen"]

	rows = []
	rows.append(["transaction size", "time-1th", "time-16th", "scal"])

	data = process_results(name, config[name], records, ["time"])
	
	for txlen in txlens:
		time1 = data[f_on][txlen][trs[0]]["med_time"]
		time16 = data[f_on][txlen][trs[1]]["med_time"]
		rows.append(["%d" % txlen, "%.4f" % time1, "%.4f" % time16, "%.4f" % (time1/time16)])

	print "Experiment: %s\n" % name
	print_csv(rows)
	print "\n\n"

def print_opacity_modes_low(records, config):
	name = "opacity_modes_low"
	f_tr = config[name]["ttr"][0]
	f_tl = config[name]["txlen"][0]

	rows = []
	rows.append(["opacity mode", "speedup"])

	data = process_results(name, config[name], records, ["time", "abort_rate"])
	baseline = data["no opacity"][f_tl][f_tr]["med_time"]

	for op in config[name]["opacity"]:
		on = run_benchmarks.opacity_names[op]
		speedup = baseline / data[on][f_tl][f_tr]["med_time"]
		rows.append([on, "%.4f" % speedup])

	print "Experiment: %s\n" % name
	print_csv(rows)
	print "\n\n"

def print_opacity_modes_high(records, config):
	name = "opacity_modes_high"
	f_tr = config[name]["ttr"][0]
	f_tl = config[name]["txlen"][0]

	rows = []
	rows.append(["opacity mode", "speedup"])

	data = process_results(name, config[name], records, ["time", "abort_rate"])
	baseline = data["no opacity"][f_tl][f_tr]["med_time"]

	for op in config[name]["opacity"]:
		on = run_benchmarks.opacity_names[op]
		speedup = baseline / data[on][f_tl][f_tr]["med_time"]
		rows.append([on, "%.4f" % speedup])

	print "Experiment: %s\n" % name
	print_csv(rows)
	print "\n\n"

def print_opacity_tl2overhead(records, config):
	name = "opacity_tl2overhead"
	tls = config[name]["txlen"]

	rows = []
	rows.append(["number of cores", "no opacity", "tl2 opacity", "slow opacity"])
	
	data = process_results(name, config[name], records, ["time", "abort_rate"])

	for tl in tls:
		baseline = data["no opacity"][tl][1]["med_time"]
		for tr in config[name]["ttr"]:
			curr_row = []
			curr_row.append("%d" % tr)
			for op in config[name]["opacity"]:
				on = run_benchmarks.opacity_names[op]
				speedup = baseline / data[on][tl][tr]["med_time"]
				curr_row.append("%.4f" % speedup)
			rows.append(curr_row)

	print "Experiment: %s\n" % name
	print_csv(rows)
	print "\n\n"

def keyf(bi, t, ntr, tl, op, ntx):
	return run_benchmarks.getRecordKey(bi, t, ntr, tl, op, ntx)

# general framework for doing statistics
def process_results(exp_name, params, records, attrs):
	processed_results = dict()
	tempdata = dict()
	bm_idx = params["exec_idx"]

	for attr in attrs:
		tempdata[attr] = []

	for opacity in params["opacity"]:
		on = run_benchmarks.opacity_names[opacity]
		processed_results[on] = dict()
		for idx in range(0, len(params["txlen"])):
			txlen = params["txlen"][idx]
			ntxs = params["ntxs"][idx]
			processed_results[on][txlen] = dict()
			for nthreads in params["ttr"]:

				# reset temp space
				for attr in attrs:
					tempdata[attr] = []

				processed_results[on][txlen][nthreads] = dict()

				for trail in range(0, ntrails):
					for attr in attrs:
						r = records[keyf(bm_idx, trail, nthreads, txlen, opacity, ntxs)][attr]
						tempdata[attr].append(r)
				for attr in attrs:
					processed_results[on][txlen][nthreads]["med_" + attr] = numpy.median(tempdata[attr])
					processed_results[on][txlen][nthreads]["err_" + attr] = [numpy.min(tempdata[attr]), numpy.max(tempdata[attr])]
					processed_results[on][txlen][nthreads]["std_" + attr] = numpy.std(tempdata[attr])

	save_processed_results(exp_name, processed_results)
	
	return processed_results

def save_processed_results(exp_name, results):
	f = open("processed_%s.json" % exp_name, "w")
	f.write(json.dumps(results, sort_keys=True, indent=2))
	f.close()

def main(argc, argv):
	if argc != 2:
		print_usage(argv[0])
		sys.exit(0)
	
	ntrails = int(argv[1])
	if ntrails <= 0 or ntrails >10:
		sys.exit(0)
	
	records = parse_exp_data_file()
	config = parse_exp_config_file()

	print_scalability_overhead(records, config)
	#print_scalability_hi_contention(records, config)
	#print_scalability_largetx(records, config)
	#print_opacity_modes_low(records, config)
	#print_opacity_modes_high(records, config)
	#print_opacity_tl2overhead(records, config)

if __name__ == "__main__":
	main(len(sys.argv), sys.argv)
