# configuration file for running experiments

import getpass
import time

def gen_stats_file():
    return "statsfile.%d" % (int(time.time() * 1e6))


# define different configurations here

USER=getpass.getuser()
STATS_COUNTERS = "checkpoint_period:persist_throughput:dbtuple_bytes_allocated:dbtuple_bytes_freed:inst_latency:commit_throughput"
NOPERSIST_STATS_COUNTERS = "inst_latency:commit_throughput"
DEFAULT_CKP_FILES = [["/f0/"+USER+"/disk1/","/f0/"+USER+"/disk2/","/f0/"+USER+"/disk3/","/f0/"+USER+"/disk4/",
                      "/f0/"+USER+"/disk5/","/f0/"+USER+"/disk6/",
                      #"/f0/"+USER+"/disk7/","/f0/"+USER+"/disk8/",
                      ],
                     ["/f1/"+USER+"/disk1/","/f1/"+USER+"/disk2/","/f1/"+USER+"/disk3/","/f1/"+USER+"/disk4/",
                      "/f1/"+USER+"/disk5/","/f1/"+USER+"/disk6/",
                      #"/f1/"+USER+"/disk7/","/f1/"+USER+"/disk8/",
                      ],
                     ["/f2/"+USER+"/disk1/","/f2/"+USER+"/disk2/","/f2/"+USER+"/disk3/","/f2/"+USER+"/disk4/",
                      "/f2/"+USER+"/disk5/","/f2/"+USER+"/disk6/",
                      #"/f2/"+USER+"/disk7/","/f2/"+USER+"/disk8/",
                      ],

                     ["/data/"+USER+"/disk1/","/data/"+USER+"/disk2/","/data/"+USER+"/disk3/","/data/"+USER+"/disk4/",
                      "/data/"+USER+"/disk5/","/data/"+USER+"/disk6/",
                      #"/data/"+USER+"/disk7/","/data/"+USER+"/disk8/",
                      ],

                     ]


ASSIGNMENT_24 = ["0,1,2,3,4,5,6,7", "8,9,10,11,12,13,14,15", "16,17,18,19,20,21,22,23"]
ASSIGNMENT_28_1 = ["0,1,2", "3,4,5,6,7,8,9,10", "11,12,13,14,15,16,17,18,19", "20,21,22,23,24,25,26,27"]
ASSIGNMENT_28_2 = ["0,1,2,3,4,5,6,7,8,9", "10,11,12,13,14,15,16,17,18", "19,20,21,22,23,24,25,26,27"]
ASSIGNMENT_28_3 = ["0,1,2,3,4,5,6", "7,8,9,10,11,12,13", "14,15,16,17,18,19,20", "21,22,23,24,25,26,27"]

TPCC_DEFAULT_WORKLOAD = "45,43,4,4,4,0"
YCSB_DEFAULT_WORKLOAD = "70,30,0,0"

DEFAULT_LOG_FILES =  ["/f0/"+USER+"/", "/f1/"+USER+"/", "/f2/"+USER+"/"]
LOG_FILE_CONFIG_1 = ["/f0/"+USER+"/", "/data/"+USER+"/", "/home/"+USER+"/disk/"]
ALL_DISK_LOG = ["/data/"+USER+"/", "/f0/"+USER+"/", "/f1/"+USER+"/", "/f2/"+USER+"/"]

DEFAULT_ROOT_DIR = "/f0/"+USER+"/"

EVENTS=True
CLEAN=False
VERBOSE=True
PERF_TIME=True

CONFIG = [

    # TPCC tests
    
    # TPCC 28 thread, without checkpointing
   {
       "runtime": 700,
       "bench": "tpcc",
       "workload" : TPCC_DEFAULT_WORKLOAD,

       "num_th": 28,
       "scale_factor": 28,
       "numa": 0,

       "logfiles": ["/data/"+USER+"/", "/f0/"+USER+"/", "/f1/"+USER+"/", "/f2/"+USER+"/"],
       "assignment": ["0,1,2,3,4,5,6","7,8,9,10,11,12,13","14,15,16,17,18,19,20","21,22,23,24,25,26,27"],

       "enable_par_ckp": False,
       "reduced_ckp": True,
       "root_dir": "/data/"+USER+"/",
       "ckpdirs": DEFAULT_CKP_FILES,

       "stats_file": gen_stats_file(),
       "stats_counters": STATS_COUNTERS,

       "run_for_ckp": 4,
       "recovery_test": False,
    
       },


   # TPCC 28 threads, with checkpointing
    {
        "runtime": 700,
        "bench": "tpcc",
        "workload" : TPCC_DEFAULT_WORKLOAD,

        "num_th": 28,
        "scale_factor": 28,
        "numa": 0,

        "logfiles": ["/data/"+USER+"/", "/f0/"+USER+"/", "/f1/"+USER+"/", "/f2/"+USER+"/"],
        "assignment": ["0,1,2,3,4,5,6","7,8,9,10,11,12,13","14,15,16,17,18,19,20","21,22,23,24,25,26,27"],

        "enable_par_ckp": True,
        "reduced_ckp": True,
        "root_dir": "/data/"+USER+"/",
        "ckpdirs": DEFAULT_CKP_FILES,

        "stats_file": gen_stats_file(),
        "stats_counters": STATS_COUNTERS,

        "run_for_ckp": 4,
        "recovery_test": False,

        },

   # TPCC 32 threads, no checkpoint
   {
       "runtime": 700,
       "bench": "tpcc",
       "workload" : TPCC_DEFAULT_WORKLOAD,

       "num_th": 32,
       "scale_factor": 32,
       "numa": 0,

       "logfiles": ["/data/"+USER+"/", "/f0/"+USER+"/", "/f1/"+USER+"/", "/f2/"+USER+"/"],
       "assignment": ["0,1,2,3,4,5,6,7","8,9,10,11,12,13,14,15","16,17,18,19,20,21,22,23","24,25,26,27,28,29,30,31"],

       "enable_par_ckp": False,
       "reduced_ckp": True,
       "root_dir": "/data/"+USER+"/",
       "ckpdirs": DEFAULT_CKP_FILES,

       "stats_file": gen_stats_file(),
       "stats_counters": STATS_COUNTERS,

       "run_for_ckp": 4,
       "recovery_test": False,
    
       },

   # TPCC 32 threads, checkpoint enabled
    {
        "runtime": 700,
        "bench": "tpcc",
        "workload" : TPCC_DEFAULT_WORKLOAD,

        "num_th": 32,
        "scale_factor": 32,
        "numa": 0,

        "logfiles": ["/data/"+USER+"/", "/f0/"+USER+"/", "/f1/"+USER+"/", "/f2/"+USER+"/"],
        "assignment": ["0,1,2,3,4,5,6,7","8,9,10,11,12,13,14,15","16,17,18,19,20,21,22,23","24,25,26,27,28,29,30,31"],

        "enable_par_ckp": True,
        "reduced_ckp": True,
        "root_dir": "/data/"+USER+"/",
        "ckpdirs": DEFAULT_CKP_FILES,

        "stats_file": gen_stats_file(),
        "stats_counters": STATS_COUNTERS,

        "run_for_ckp": 4,
        "recovery_test": False,

        },

   # TPCC mem only, 28 threads
    {
        "runtime": 700,
        "bench": "tpcc",
        "workload" : TPCC_DEFAULT_WORKLOAD,

        "num_th": 28,
        "scale_factor": 28,
        "numa": 0,

        "logfiles": "",
        "assignment": "",

        "enable_par_ckp": False,
        "reduced_ckp": False,
        "root_dir": "/f0/"+USER+"/",
        "ckpdirs": "",

        "stats_file": gen_stats_file(),
        "stats_counters": NOPERSIST_STATS_COUNTERS,

        "run_for_ckp": 0,
        "recovery_test": False,

        },

   # TPCC mem only, 32 threads
    {
        "runtime": 700,
        "bench": "tpcc",
        "workload" : TPCC_DEFAULT_WORKLOAD,

        "num_th": 32,
        "scale_factor": 32,
        "numa": 0,

        "logfiles": "",
        "assignment": "",

        "enable_par_ckp": False,
        "reduced_ckp": False,
        "root_dir": "/f0/"+USER+"/",
        "ckpdirs": "",

        "stats_file": gen_stats_file(),
        "stats_counters": NOPERSIST_STATS_COUNTERS,

        "run_for_ckp": 0,
        "recovery_test": False,

        },

   # YCSB tests

   # YCSB 28 threads, with no checkpoint
    {
        "runtime": 700,
        "bench": "ycsb",
        "workload" : YCSB_DEFAULT_WORKLOAD,

        "num_th": 28,
        "scale_factor": 400000,
        "numa": 0,

        "logfiles": ["/data/"+USER+"/", "/f0/"+USER+"/", "/f1/"+USER+"/", "/f2/"+USER+"/"],
        "assignment": ["0,1,2,3,4,5,6","7,8,9,10,11,12,13","14,15,16,17,18,19,20","21,22,23,24,25,26,27"],

        "enable_par_ckp": False,
        "reduced_ckp": True,
        "root_dir": "/data/"+USER+"/",
        "ckpdirs": DEFAULT_CKP_FILES,

        "stats_file": gen_stats_file(),
        "stats_counters": STATS_COUNTERS,
        
        "run_for_ckp": 6,
        "recovery_test": False,

        },

   # YCSB 28 threads, checkpoint enabled
    {
        "runtime": 700,
        "bench": "ycsb",
        "workload" : YCSB_DEFAULT_WORKLOAD,

        "num_th": 28,
        "scale_factor": 400000,
        "numa": 0,

        "logfiles": ["/data/"+USER+"/", "/f0/"+USER+"/", "/f1/"+USER+"/", "/f2/"+USER+"/"],
        "assignment": ["0,1,2,3,4,5,6","7,8,9,10,11,12,13","14,15,16,17,18,19,20","21,22,23,24,25,26,27"],

        "enable_par_ckp": True,
        "reduced_ckp": True,
        "root_dir": "/data/"+USER+"/",
        "ckpdirs": DEFAULT_CKP_FILES,

        "stats_file": gen_stats_file(),
        "stats_counters": STATS_COUNTERS,

        "run_for_ckp": 6,
        "recovery_test": False,

        },

   # YCSB 32 threads, with no checkpoint
    {
        "runtime": 700,
        "bench": "ycsb",
        "workload" : YCSB_DEFAULT_WORKLOAD,

        "num_th": 32,
        "scale_factor": 400000,
        "numa": 0,

        "logfiles": ["/data/"+USER+"/", "/f0/"+USER+"/", "/f1/"+USER+"/", "/f2/"+USER+"/"],
        "assignment": ["0,1,2,3,4,5,6,7","8,9,10,11,12,13,14,15","16,17,18,19,20,21,22,23","24,25,26,27,28,29,30,31"],

        "enable_par_ckp": False,
        "reduced_ckp": True,
        "root_dir": "/data/"+USER+"/",
        "ckpdirs": DEFAULT_CKP_FILES,

        "stats_file": gen_stats_file(),
        "stats_counters": STATS_COUNTERS,
        
        "run_for_ckp": 6,
        "recovery_test": False,

        },

   # YCSB 32 threads, checkpoint enabled
    {
        "runtime": 700,
        "bench": "ycsb",
        "workload" : YCSB_DEFAULT_WORKLOAD,

        "num_th": 32,
        "scale_factor": 400000,
        "numa": 0,

        "logfiles": ["/data/"+USER+"/", "/f0/"+USER+"/", "/f1/"+USER+"/", "/f2/"+USER+"/"],
        "assignment": ["0,1,2,3,4,5,6,7","8,9,10,11,12,13,14,15","16,17,18,19,20,21,22,23","24,25,26,27,28,29,30,31"],

        "enable_par_ckp": True,
        "reduced_ckp": True,
        "root_dir": "/data/"+USER+"/",
        "ckpdirs": DEFAULT_CKP_FILES,

        "stats_file": gen_stats_file(),
        "stats_counters": STATS_COUNTERS,

        "run_for_ckp": 6,
        "recovery_test": False,

        },


   # YCSB memory only, 28 threads
    {
        "runtime": 700,
        "bench": "ycsb",
        "workload" : YCSB_DEFAULT_WORKLOAD,

        "num_th": 28,
        "scale_factor": 400000,
        "numa": 0,

        "logfiles": "",
        "assignment": "",

        "enable_par_ckp": False,
        "reduced_ckp": False,
        "root_dir": "/f0/"+USER+"/",
        "ckpdirs": "",

        "stats_file": gen_stats_file(),
        "stats_counters": NOPERSIST_STATS_COUNTERS,

        "run_for_ckp": 0,
        "recovery_test": False,

        },

   # YCSB memory only, 32 threads
    {
        "runtime": 700,
        "bench": "ycsb",
        "workload" : YCSB_DEFAULT_WORKLOAD,

        "num_th": 32,
        "scale_factor": 400000,
        "numa": 0,

        "logfiles": "",
        "assignment": "",

        "enable_par_ckp": False,
        "reduced_ckp": False,
        "root_dir": "/f0/"+USER+"/",
        "ckpdirs": "",

        "stats_file": gen_stats_file(),
        "stats_counters": NOPERSIST_STATS_COUNTERS,

        "run_for_ckp": 0,
        "recovery_test": False,

        },

]
