# Parses the json results of running run_benchmark.py for the boosting 
# microbenchmark. Expects the json filename as an argument.

import sys
import json
from collections import defaultdict

with open(sys.argv[1], 'r') as f:
    d = json.loads(f.read())

names = {"2": "sto", "3": "boostingsto", "4": "boostingstandalone"}
output = defaultdict(lambda: {"results": []})
for k, v in d.iteritems():
    idx = k.split('/')[0]
    name = names[idx]
    output[name]["results"].append(v["time"])

for k, v in output.iteritems():
    res = sorted(v["results"])
    v["results"] = res
    v["median"] = res[len(res)/2]
    v["min"] = res[0]
    v["max"] = res[-1]

print json.dumps(output, indent=2)
