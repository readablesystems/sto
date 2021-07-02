#!/bin/bash

python3 struct_generator.py test/unit-adaptive-structs.ini -n :: -o test/unit-adaptive-structs-generated.hh
python3 struct_generator.py benchmark/Dynamic_structs.ini -n :: -o benchmark/Dynamic_structs_generated.hh
python3 struct_generator.py benchmark/TPCC_structs.ini -n :: -o benchmark/TPCC_structs_generated.hh
python3 struct_generator.py benchmark/YCSB_structs.ini -n ::ycsb -o benchmark/YCSB_structs_generated.hh
