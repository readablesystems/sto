#!/bin/bash

## Experiment setup documentation
#
# (Sorted in lexicographical order by setup function name)
#
# setup_rubis: RUBiS
# setup_tpcc: TPC-C, 1, 4, and scaling (#wh = #th) warehouses, OCC and MVCC
# setup_tpcc_gc: TPC-C, 1 and scaling warehouses, gc cycle of 1ms, 100ms, 10s (off)
# setup_tpcc_mvcc: TPC-C, 1, 4, and scaling warehouses, MVCC only
# setup_tpcc_occ: TPC-C, 1, 4, and scaling warehouses, OCC only
# setup_tpcc_opacity: TPC-C with opacity, 1, 4, and scaling warehouses
# setup_tpcc_safe_flatten: TPC-C with safer flattening MVCC, 1, 4, and scaling warehouses
# setup_tpcc_scaled: TPC-C, #warehouses = #threads
# setup_tpcc_tictoc: TPC-C, 1, 4, and scaling warehouses, using TicToc
# setup_tpcc_noncumu_factors: (see below)
# setup_tpcc_noncumu_factors_occ: (see below)
# setup_tpcc_mvcc_cu: TPC-C CU read at present vs past.
# setup_tpcc_stacked_factors_mvcc: TPC-C non-cumulative factors.
# setup_tpcc_stacked_factors: (see below)
# setup_tpcc_stacked_factors_occ: (see below)
# setup_tpcc_stacked_factors_mvcc: TPC-C factor analysis experiments.
# setup_tpcc_occ_idx_cont: TPC-C OCC index contention.
# setup_tpcc_idx_cont: TPC-C index contention.
# setup_wiki: Wikipedia
# setup_ycsba: YCSB-A
# setup_ycsba_occ: YCSB-A, OCC only
# setup_ycsba_tictoc: YCSB-A, TicToc
# setup_ycsba_mvcc: YCSB-A, OCC only
# setup_ycsba_semopts: YCSB-A semantic optimizations comparison
# setup_ycsbb: YCSB-B
# setup_ycsbb_occ: YCSB-B, OCC only
# setup_ycsbb_tictoc: YCSB-B, TicToc
# setup_ycsbb_mvcc: YCSB-B, MVCC only
# setup_ycsbb_semopts: YCSB-B semantic optimizations comparison
# setup_ycsbc: YCSB-C
# setup_ycsbc_semopts: YCSB-C semantic optimizations comparison
# setup_ycsbx_semopts: YCSB Collapse on Writers + R/O semantic optimizations comparison
# setup_ycsby_semopts: YCSB Collapse on Writers + R/W semantic optimizations comparison
# setup_ycsbz_semopts: YCSB Collapse on R/W + Writers semantic optimizations comparison

setup_rubis() {
  EXPERIMENT_NAME="RUBiS"
  ITERS=10

  RUBIS_OCC=(
    "OCC"         "-idefault -s1.0 -g"
    "OCC + CU"    "-idefault -s1.0 -g -x"
  )

  RUBIS_MVCC=(
    "MVCC"        "-imvcc -s1.0 -g"
    "MVCC + CU"   "-imvcc -s1.0 -g -x"
  )

  RUBIS_OCC_BINARIES=(
    "rubis_bench" "-occ" "NDEBUG=1 FINE_GRAINED=1" " + SV"
  )
  RUBIS_MVCC_BINARIES=(
    "rubis_bench" "-mvcc" "NDEBUG=1 FINE_GRAINED=1 INLINED_VERSIONS=1" " + SV"
  )
  RUBIS_BOTH_BINARIES=(
    "rubis_bench" "-both" "NDEBUG=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=("${RUBIS_OCC[@]}")
  MVCC_LABELS=("${RUBIS_MVCC[@]}")
  OCC_BINARIES=("${RUBIS_OCC_BINARIES[@]}" "${RUBIS_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${RUBIS_MVCC_BINARIES[@]}" "${RUBIS_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_rubis_occ() {
  EXPERIMENT_NAME="RUBiS (OCC only)"
  ITERS=10

  RUBIS_OCC=(
    "OCC"         "-idefault -s1.0 -g"
    "OCC + CU"    "-idefault -s1.0 -g -x"
  )

  RUBIS_MVCC=(
  )

  RUBIS_OCC_BINARIES=(
    "rubis_bench" "-occ" "NDEBUG=1 FINE_GRAINED=1" " + SV"
  )
  RUBIS_MVCC_BINARIES=(
  )
  RUBIS_BOTH_BINARIES=(
    "rubis_bench" "-both" "NDEBUG=1" ""
  )

  OCC_LABELS=("${RUBIS_OCC[@]}")
  MVCC_LABELS=("${RUBIS_MVCC[@]}")
  OCC_BINARIES=("${RUBIS_OCC_BINARIES[@]}" "${RUBIS_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${RUBIS_MVCC_BINARIES[@]}" "${RUBIS_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_rubis_mvcc() {
  EXPERIMENT_NAME="RUBiS MVCC only (integrated TS)"
  ITERS=10

  RUBIS_OCC=(
  )

  RUBIS_MVCC=(
    "MVCC"        "-imvcc -s1.0 -g"
    "MVCC + CU"   "-imvcc -s1.0 -g -x"
  )

  RUBIS_OCC_BINARIES=(
  )
  RUBIS_MVCC_BINARIES=(
    "rubis_bench" "-mvcc" "NDEBUG=1 FINE_GRAINED=1 INLINED_VERSIONS=1" " + SV"
  )
  RUBIS_BOTH_BINARIES=(
    "rubis_bench" "-both" "NDEBUG=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=("${RUBIS_OCC[@]}")
  MVCC_LABELS=("${RUBIS_MVCC[@]}")
  OCC_BINARIES=("${RUBIS_OCC_BINARIES[@]}" "${RUBIS_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${RUBIS_MVCC_BINARIES[@]}" "${RUBIS_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_rubis_tictoc() {
  EXPERIMENT_NAME="RUBiS, TicToc"
  ITERS=10

  RUBIS_OCC=(
    "TicToc"      "-itictoc -s1.0 -g"
    "TicToc + CU" "-itictoc -s1.0 -g -x"
  )

  RUBIS_MVCC=(
  )

  RUBIS_OCC_BINARIES=(
    "rubis_bench" "-occ" "NDEBUG=1 FINE_GRAINED=1" " + SV"
  )
  RUBIS_MVCC_BINARIES=(
  )
  RUBIS_BOTH_BINARIES=(
    "rubis_bench" "-both" "NDEBUG=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=("${RUBIS_OCC[@]}")
  MVCC_LABELS=("${RUBIS_MVCC[@]}")
  OCC_BINARIES=("${RUBIS_OCC_BINARIES[@]}" "${RUBIS_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${RUBIS_MVCC_BINARIES[@]}" "${RUBIS_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_tpcc() {
  EXPERIMENT_NAME="TPC-C"

  TPCC_OCC=(
    "OCC (W1)"         "-idefault -g -w1"
    "OCC + CU (W1)"    "-idefault -g -x -w1"
    "OCC (W4)"         "-idefault -g -w4"
    "OCC + CU (W4)"    "-idefault -g -x -w4"
    "OCC (W0)"         "-idefault -g"
    "OCC + CU (W0)"    "-idefault -g -x"
  )

  TPCC_MVCC=(
    "MVCC (W1)"        "-imvcc -g -w1"
    "MVCC + CU (W1)"   "-imvcc -g -x -w1"
    "MVCC (W4)"        "-imvcc -g -w4"
    "MVCC + CU (W4)"   "-imvcc -g -x -w4"
    "MVCC (W0)"        "-imvcc -g"
    "MVCC + CU (W0)"   "-imvcc -g -x"
  )

  TPCC_OCC_BINARIES=(
    "tpcc_bench" "-occ" "NDEBUG=1 OBSERVE_C_BALANCE=1 FINE_GRAINED=1" " + SV"
  )
  TPCC_MVCC_BINARIES=(
    "tpcc_bench" "-mvcc" "NDEBUG=1 OBSERVE_C_BALANCE=1 FINE_GRAINED=1 INLINED_VERSIONS=1" " + SV"
  )
  TPCC_BOTH_BINARIES=(
    "tpcc_bench" "-both" "NDEBUG=1 OBSERVE_C_BALANCE=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=("${TPCC_OCC[@]}")
  MVCC_LABELS=("${TPCC_MVCC[@]}")
  OCC_BINARIES=("${TPCC_OCC_BINARIES[@]}" "${TPCC_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${TPCC_MVCC_BINARIES[@]}" "${TPCC_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    if [[ $cmd != *"-w"* ]]
    then
      cmd="$cmd -w$i"
    fi
  }
}

setup_tpcc_gc() {
  EXPERIMENT_NAME="TPC-C, variable GC"

  TPCC_OCC=(
  )

  TPCC_MVCC=(
    "MVCC (W1; R1000)"     "-imvcc -g -r1000 -w1"
    "MVCC (W0; R1000)"     "-imvcc -g -r1000"
    "MVCC (W1; R100000)"   "-imvcc -g -r100000 -w1"
    "MVCC (W0; R100000)"   "-imvcc -g -r100000"
    "MVCC (W1; R0)"        "-imvcc -w1"
    "MVCC (W0; R0)"        "-imvcc"
  )

  TPCC_OCC_BINARIES=(
  )
  TPCC_MVCC_BINARIES=(
  )
  TPCC_BOTH_BINARIES=(
    "tpcc_bench" "-both" "DEBUG=1 OBSERVE_C_BALANCE=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=("${TPCC_OCC[@]}")
  MVCC_LABELS=("${TPCC_MVCC[@]}")
  OCC_BINARIES=("${TPCC_OCC_BINARIES[@]}" "${TPCC_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${TPCC_MVCC_BINARIES[@]}" "${TPCC_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    if [[ $cmd != *"-w"* ]]
    then
      cmd="$cmd -w$i"
    fi
  }
}

setup_tpcc_mvcc() {
  EXPERIMENT_NAME="TPC-C MVCC (100ms GC)"

  TPCC_OCC=(
  )

  TPCC_MVCC=(
    "MVCC (W1)"        "-imvcc -g -w1"
    "MVCC + CU (W1)"   "-imvcc -g -x -w1"
    "MVCC (W4)"        "-imvcc -g -w4"
    "MVCC + CU (W4)"   "-imvcc -g -x -w4"
    "MVCC (W0)"        "-imvcc -g"
    "MVCC + CU (W0)"   "-imvcc -g -x"
  )

  TPCC_OCC_BINARIES=(
  )
  TPCC_MVCC_BINARIES=(
    "tpcc_bench" "-mvcc" "NDEBUG=1 FINE_GRAINED=1 INLINED_VERSIONS=1" " + SV"
  )
  TPCC_BOTH_BINARIES=(
    "tpcc_bench" "-both" "NDEBUG=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=()
  MVCC_LABELS=("${TPCC_MVCC[@]}")
  OCC_BINARIES=()
  MVCC_BINARIES=("${TPCC_MVCC_BINARIES[@]}" "${TPCC_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    if [[ $cmd != *"-w"* ]]
    then
      cmd="$cmd -w$i"
    fi
  }
}

setup_tpcc_mvcc_1gc() {
  EXPERIMENT_NAME="TPC-C MVCC (1ms GC, integrated TS)"

  TPCC_OCC=(
  )

  TPCC_MVCC=(
    "MVCC (W1)"        "-imvcc -g -w1 -r1000"
    "MVCC + CU (W1)"   "-imvcc -g -x -w1 -r1000"
    "MVCC (W4)"        "-imvcc -g -w4 -r1000"
    "MVCC + CU (W4)"   "-imvcc -g -x -w4 -r1000"
    "MVCC (W0)"        "-imvcc -g -r1000"
    "MVCC + CU (W0)"   "-imvcc -g -x -r1000"
  )

  TPCC_OCC_BINARIES=(
  )
  TPCC_MVCC_BINARIES=(
    "tpcc_bench" "-mvcc" "NDEBUG=1 FINE_GRAINED=1 INLINED_VERSIONS=1" " + SV"
  )
  TPCC_BOTH_BINARIES=(
    "tpcc_bench" "-both" "NDEBUG=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=()
  MVCC_LABELS=("${TPCC_MVCC[@]}")
  OCC_BINARIES=()
  MVCC_BINARIES=("${TPCC_MVCC_BINARIES[@]}" "${TPCC_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    if [[ $cmd != *"-w"* ]]
    then
      cmd="$cmd -w$i"
    fi
  }
}

setup_tpcc_mvcc_vp_1gc() {
  EXPERIMENT_NAME="TPC-C MVCC (1ms GC, vertical partitioning, REQUIRES MASTER BRANCH)"

  TPCC_OCC=(
  )

  TPCC_MVCC=(
    "MVCC (W1)"        "-imvcc -g -w1 -r1000"
    "MVCC + CU (W1)"   "-imvcc -g -x -w1 -r1000"
    "MVCC (W4)"        "-imvcc -g -w4 -r1000"
    "MVCC + CU (W4)"   "-imvcc -g -x -w4 -r1000"
    "MVCC (W0)"        "-imvcc -g -r1000"
    "MVCC + CU (W0)"   "-imvcc -g -x -r1000"
  )

  TPCC_OCC_BINARIES=(
  )
  TPCC_MVCC_BINARIES=(
    "tpcc_bench" "-mvcc" "NDEBUG=1 FINE_GRAINED=1 INLINED_VERSIONS=1" " + SV"
  )
  TPCC_BOTH_BINARIES=(
    "tpcc_bench" "-both" "NDEBUG=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=()
  MVCC_LABELS=("${TPCC_MVCC[@]}")
  OCC_BINARIES=()
  MVCC_BINARIES=("${TPCC_MVCC_BINARIES[@]}" "${TPCC_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    if [[ $cmd != *"-w"* ]]
    then
      cmd="$cmd -w$i"
    fi
  }
}

setup_tpcc_mvcc_nots_1gc() {
  EXPERIMENT_NAME="TPC-C MVCC, no TS (1ms GC)"

  TPCC_OCC=(
  )

  TPCC_MVCC=(
    "MVCC (W1)"        "-imvcc -g -w1 -r1000"
    "MVCC + CU (W1)"   "-imvcc -g -x -w1 -r1000"
    "MVCC (W4)"        "-imvcc -g -w4 -r1000"
    "MVCC + CU (W4)"   "-imvcc -g -x -w4 -r1000"
    "MVCC (W0)"        "-imvcc -g -r1000"
    "MVCC + CU (W0)"   "-imvcc -g -x -r1000"
  )

  TPCC_OCC_BINARIES=(
  )
  TPCC_MVCC_BINARIES=(
  )
  TPCC_BOTH_BINARIES=(
    "tpcc_bench" "-both" "NDEBUG=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=()
  MVCC_LABELS=("${TPCC_MVCC[@]}")
  OCC_BINARIES=()
  MVCC_BINARIES=("${TPCC_MVCC_BINARIES[@]}" "${TPCC_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    if [[ $cmd != *"-w"* ]]
    then
      cmd="$cmd -w$i"
    fi
  }
}

setup_tpcc_mvcc_ts_1gc() {
  EXPERIMENT_NAME="TPC-C MVCC, with TS (1ms GC)"

  TPCC_OCC=(
  )

  TPCC_MVCC=(
    "MVCC (W1)"        "-imvcc -g -w1 -r1000"
    "MVCC + CU (W1)"   "-imvcc -g -x -w1 -r1000"
    "MVCC (W4)"        "-imvcc -g -w4 -r1000"
    "MVCC + CU (W4)"   "-imvcc -g -x -w4 -r1000"
    "MVCC (W0)"        "-imvcc -g -r1000"
    "MVCC + CU (W0)"   "-imvcc -g -x -r1000"
  )

  TPCC_OCC_BINARIES=(
  )
  TPCC_MVCC_BINARIES=(
    "tpcc_bench" "-mvcc" "NDEBUG=1 FINE_GRAINED=1 INLINED_VERSIONS=1" " + SV"
  )
  TPCC_BOTH_BINARIES=(
  )

  OCC_LABELS=()
  MVCC_LABELS=("${TPCC_MVCC[@]}")
  OCC_BINARIES=()
  MVCC_BINARIES=("${TPCC_MVCC_BINARIES[@]}" "${TPCC_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    if [[ $cmd != *"-w"* ]]
    then
      cmd="$cmd -w$i"
    fi
  }
}

setup_tpcc_occ() {
  EXPERIMENT_NAME="TPC-C OCC"

  TPCC_OCC=(
    "OCC (W1)"         "-idefault -g -w1 -r1000"
    "OCC + CU (W1)"    "-idefault -g -x -w1 -r1000"
    "OCC (W4)"         "-idefault -g -w4 -r1000"
    "OCC + CU (W4)"    "-idefault -g -x -w4 -r1000"
    "OCC (W0)"         "-idefault -g -r1000"
    "OCC + CU (W0)"    "-idefault -g -x -r1000"
  )

  TPCC_MVCC=(
  )

  TPCC_OCC_BINARIES=(
    "tpcc_bench" "-occ" "NDEBUG=1 OBSERVE_C_BALANCE=1 FINE_GRAINED=1" " + SV"
  )
  TPCC_MVCC_BINARIES=(
  )
  TPCC_BOTH_BINARIES=(
    "tpcc_bench" "-both" "NDEBUG=1 OBSERVE_C_BALANCE=1" ""
  )

  OCC_LABELS=("${TPCC_OCC[@]}")
  MVCC_LABELS=()
  OCC_BINARIES=("${TPCC_OCC_BINARIES[@]}" "${TPCC_BOTH_BINARIES[@]}")
  MVCC_BINARIES=()

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    if [[ $cmd != *"-w"* ]]
    then
      cmd="$cmd -w$i"
    fi
  }
}

setup_tpcc_opacity() {
  EXPERIMENT_NAME="TPC-C with Opacity"

  TPCC_OCC=(
    "OPQ (W1)"         "-iopaque -g -w1"
    "OPQ + CU (W1)"    "-iopaque -g -w1 -x"
    "OPQ (W4)"         "-iopaque -g -w4"
    "OPQ + CU (W4)"    "-iopaque -g -x -w4"
    "OPQ (W0)"         "-iopaque -g"
    "OPQ + CU (W0)"    "-iopaque -g -x"
  )

  TPCC_MVCC=(
  )

  TPCC_OCC_BINARIES=(
    "tpcc_bench" "-occ" "NDEBUG=1 OBSERVE_C_BALANCE=1 FINE_GRAINED=1" " + SV"
  )
  TPCC_MVCC_BINARIES=(
  )
  TPCC_BOTH_BINARIES=(
    "tpcc_bench" "-both" "NDEBUG=1 OBSERVE_C_BALANCE=1" ""
  )

  OCC_LABELS=("${TPCC_OCC[@]}")
  MVCC_LABELS=()
  OCC_BINARIES=("${TPCC_OCC_BINARIES[@]}" "${TPCC_BOTH_BINARIES[@]}")
  MVCC_BINARIES=()

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    if [[ $cmd != *"-w"* ]]
    then
      cmd="$cmd -w$i"
    fi
  }
}

setup_tpcc_safe_flatten() {
  EXPERIMENT_NAME="TPC-C MVCC aborting unsafe flattens in CU"

  TPCC_OCC=(
  )

  TPCC_MVCC=(
    "MVCC (W1)"         "-imvcc -g -w1"
    "MVCC + CU (W1)"    "-imvcc -g -w1 -x"
    "MVCC (W4)"         "-imvcc -g -w4"
    "MVCC + CU (W4)"    "-imvcc -g -x -w4"
    "MVCC (W0)"         "-imvcc -g"
    "MVCC + CU (W0)"    "-imvcc -g -x"
  )

  TPCC_OCC_BINARIES=(
  )
  TPCC_MVCC_BINARIES=(
    "tpcc_bench" "-mvcc" "NDEBUG=1 INLINED_VERSIONS=1 FINE_GRAINED=1 OBSERVE_C_BALANCE=1 SAFE_FLATTEN=1" " + SV"
  )
  TPCC_BOTH_BINARIES=(
    "tpcc_bench" "-both" "NDEBUG=1 INLINED_VERSIONS=1 OBSERVE_C_BALANCE=1 SAFE_FLATTEN=1" ""
  )

  OCC_LABELS=()
  MVCC_LABELS=("${TPCC_MVCC[@]}")
  OCC_BINARIES=()
  MVCC_BINARIES=("${TPCC_MVCC_BINARIES[@]}" "${TPCC_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    if [[ $cmd != *"-w"* ]]
    then
      cmd="$cmd -w$i"
    fi
  }
}

setup_tpcc_scaled() {
  EXPERIMENT_NAME="TPC-C, #W = #T"

  TPCC_OCC=(
    "OCC (W0)"         "-idefault -g"
    "OCC + CU (W0)"    "-idefault -g -x"
    "OPQ (W0)"         "-iopaque -g"
    "OPQ +CU (W0)"     "-iopaque -g -x"
  )

  TPCC_MVCC=(
    "MVCC (W0)"        "-imvcc -g"
    "MVCC +CU (W0)"    "-imvcc -g -x"
  )

  TPCC_OCC_BINARIES=(
    "tpcc_bench" "-occ" "NDEBUG=1 OBSERVE_C_BALANCE=1 FINE_GRAINED=1" " + SV"
  )
  TPCC_MVCC_BINARIES=(
    "tpcc_bench" "-mvcc" "NDEBUG=1 OBSERVE_C_BALANCE=1 FINE_GRAINED=1 INLINED_VERSIONS=1" " + SV"
  )
  TPCC_BOTH_BINARIES=(
    "tpcc_bench" "-both" "NDEBUG=1 OBSERVE_C_BALANCE=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=("${TPCC_OCC[@]}")
  MVCC_LABELS=("${TPCC_MVCC[@]}")
  OCC_BINARIES=("${TPCC_OCC_BINARIES[@]}" "${TPCC_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${TPCC_MVCC_BINARIES[@]}" "${TPCC_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    cmd="$cmd -w$i"
  }
}

setup_tpcc_history_key() {
  EXPERIMENT_NAME="TPC-C History Table Sequential Insert Experiments (OCC and TicToc)"

  TPCC_OCC=(
    "OCC (W1)"         "-idefault -g -w1 -r1000"
    "OCC + CU (W1)"    "-idefault -g -w1 -x -r1000"
    "TicToc (W1)"      "-itictoc -n -g -w1 -r1000"
    "TicToc + CU (W1)" "-itictoc -n -g -w1 -x -r1000"
  )

  TPCC_MVCC=(
  )

  TPCC_OCC_BINARIES=(
  )
  TPCC_MVCC_BINARIES=(
  )
  TPCC_BOTH_BINARIES=(
    "tpcc_bench" "-seq"    "NDEBUG=1 OBSERVE_C_BALANCE=1 HISTORY_SEQ_INSERT=1" " + SEQ"
    "tpcc_bench" "-ts-seq" "NDEBUG=1 OBSERVE_C_BALANCE=1 FINE_GRAINED=1 HISTORY_SEQ_INSERT=1" " + TS + SEQ"
  )

  OCC_LABELS=("${TPCC_OCC[@]}")
  MVCC_LABELS=()
  OCC_BINARIES=("${TPCC_BOTH_BINARIES[@]}")
  MVCC_BINARIES=()

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    if [[ $cmd != *"-w"* ]]
    then
      cmd="$cmd -w$i"
    fi
  }
}

setup_tpcc_tictoc_full() {
  EXPERIMENT_NAME="TPC-C TicToc (full phantom protection)"

  TPCC_OCC=(
    "TicToc (W1)"      "-itictoc -n -g -w1 -r1000"
    "TicToc + CU (W1)" "-itictoc -n -g -w1 -x -r1000"
    "TicToc (W4)"      "-itictoc -n -g -w4 -r1000"
    "TicToc + CU (W4)" "-itictoc -n -g -w4 -x -r1000"
    "TicToc (W0)"      "-itictoc -n -g -r1000"
    "TicToc + CU (W0)" "-itictoc -n -g -x -r1000"
  )

  TPCC_MVCC=(
  )

  TPCC_OCC_BINARIES=(
  )
  TPCC_MVCC_BINARIES=(
  )
  TPCC_BOTH_BINARIES=(
    "tpcc_bench" "-both" "NDEBUG=1 OBSERVE_C_BALANCE=1" ""
    "tpcc_bench" "-tsplit" "NDEBUG=1 OBSERVE_C_BALANCE=1 FINE_GRAINED=1" " + SV"
  )

  OCC_LABELS=("${TPCC_OCC[@]}")
  MVCC_LABELS=()
  OCC_BINARIES=("${TPCC_BOTH_BINARIES[@]}")
  MVCC_BINARIES=()

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    if [[ $cmd != *"-w"* ]]
    then
      cmd="$cmd -w$i"
    fi
  }
}

setup_tpcc_tictoc_incorrect() {
  EXPERIMENT_NAME="TPC-C TicToc (incorrect)"

  TPCC_OCC=(
    "TicToc (W1)"      "-itictoc -g -w1 -r1000"
    "TicToc + CU (W1)" "-itictoc -g -w1 -x -r1000"
    "TicToc (W4)"      "-itictoc -g -w4 -r1000"
    "TicToc + CU (W4)" "-itictoc -g -w4 -x -r1000"
    "TicToc (W0)"      "-itictoc -g -r1000"
    "TicToc + CU (W0)" "-itictoc -g -x -r1000"
  )

  TPCC_MVCC=(
  )

  TPCC_OCC_BINARIES=(
  )
  TPCC_MVCC_BINARIES=(
  )
  TPCC_BOTH_BINARIES=(
    "tpcc_bench" "-both" "NDEBUG=1 OBSERVE_C_BALANCE=1" ""
    "tpcc_bench" "-tsplit" "NDEBUG=1 OBSERVE_C_BALANCE=1 FINE_GRAINED=1" " + SV"
  )

  OCC_LABELS=("${TPCC_OCC[@]}")
  MVCC_LABELS=()
  OCC_BINARIES=("${TPCC_BOTH_BINARIES[@]}")
  MVCC_BINARIES=()

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    if [[ $cmd != *"-w"* ]]
    then
      cmd="$cmd -w$i"
    fi
  }
}

setup_tpcc_factors() {
  EXPERIMENT_NAME="TPC-C Factors (1W 24T MV)"

  TPCC_OCC=(
  )

  TPCC_MVCC=(
    "MVCC (W1)"          "-imvcc -g -w1 -r1000"
  )

  TPCC_OCC_BINARIES=(
  )
  TPCC_MVCC_BINARIES=(
  )
  TPCC_BOTH_BINARIES=(
    "tpcc_bench" "-noall" "NDEBUG=1 INLINED_VERSIONS=1 USE_HASH_INDEX=0 CONTENTION_REG=0 USE_LIBCMALLOC=1" "-AL-BACKOFF-HT"
    "tpcc_bench" "-noht"  "NDEBUG=1 INLINED_VERSIONS=1 USE_HASH_INDEX=0"                                   "-HT"
    "tpcc_bench" "-noal"  "NDEBUG=1 INLINED_VERSIONS=1 USE_LIBCMALLOC=1"                                   "-AL"
    "tpcc_bench" "-exp"   "NDEBUG=1 INLINED_VERSIONS=1 USE_EXCEPTION=1"                                    "-NOEXP"
    "tpcc_bench" "-noreg" "NDEBUG=1 INLINED_VERSIONS=1 CONTENTION_REG=0"                                   "-BACKOFF"
    "tpcc_bench" "-base"  "NDEBUG=1 INLINED_VERSIONS=1"                                                    ""
  )

  OCC_LABELS=()
  MVCC_LABELS=("${TPCC_MVCC[@]}")
  OCC_BINARIES=()
  MVCC_BINARIES=("${TPCC_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_tpcc_noncumu_factors() {
  EXPERIMENT_NAME="TPC-C Non-cumulative Factors - All"

  TPCC_OCC=(
    "OCC (W1)" "-idefault -g -w1"
    "OCC (W4)" "-idefault -g -w4"
    "OCC (W0)" "-idefault -g"
    "TicToc (W1)"      "-itictoc -g -w1"
    "TicToc (W4)"      "-itictoc -g -w4"
    "TicToc (W0)"      "-itictoc -g"
  )

  TPCC_MVCC=(
    "MVCC (W1)" "-imvcc -g -w1"
    "MVCC (W4)" "-imvcc -g -w4"
    "MVCC (W0)" "-imvcc -g"
  )

  TPCC_OCC_BINARIES=(
  )
  TPCC_MVCC_BINARIES=(
  )
  TPCC_BOTH_BINARIES=(
    "tpcc_bench" "-f1" "NDEBUG=1 INLINED_VERSIONS=1 USE_LIBCMALLOC=1" "-AL"
    "tpcc_bench" "-f2" "NDEBUG=1 INLINED_VERSIONS=1 USE_EXCEPTION=1"  "-NOEXC"
    "tpcc_bench" "-f3" "NDEBUG=1 INLINED_VERSIONS=1 CONTENTION_REG=0" "-BACKOFF"
    "tpcc_bench" "-f4" "NDEBUG=1 INLINED_VERSIONS=1 USE_HASH_INDEX=0" "-HASH"
    "tpcc_bench" "-f0" "NDEBUG=1 INLINED_VERSIONS=1"                  "BASE"
  )

  OCC_LABELS=("${TPCC_OCC[@]}")
  MVCC_LABELS=("${TPCC_MVCC[@]}")
  OCC_BINARIES=("${TPCC_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${TPCC_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    if [[ $cmd != *"-w"* ]]
    then
      cmd="$cmd -w$i"
    fi
  }
}

setup_tpcc_noncumu_factors_occ() {
  EXPERIMENT_NAME="TPC-C Non-cumulative Factors (OCC-only)"

  TPCC_OCC=(
    "OCC (W1)" "-idefault -g -w1"
    "OCC (W4)" "-idefault -g -w4"
    "OCC (W0)" "-idefault -g"
  )

  TPCC_MVCC=(
  )

  TPCC_OCC_BINARIES=(
  )
  TPCC_MVCC_BINARIES=(
  )
  TPCC_BOTH_BINARIES=(
    "tpcc_bench" "-f1" "NDEBUG=1 INLINED_VERSIONS=1 USE_LIBCMALLOC=1" "-AL"
    "tpcc_bench" "-f2" "NDEBUG=1 INLINED_VERSIONS=1 USE_EXCEPTION=1"  "-NOEXC"
    "tpcc_bench" "-f3" "NDEBUG=1 INLINED_VERSIONS=1 CONTENTION_REG=0" "-BACKOFF"
    "tpcc_bench" "-f4" "NDEBUG=1 INLINED_VERSIONS=1 USE_HASH_INDEX=0" "-HASH"
    "tpcc_bench" "-f0" "NDEBUG=1 INLINED_VERSIONS=1"                  "BASE"
  )

  OCC_LABELS=("${TPCC_OCC[@]}")
  MVCC_LABELS=("${TPCC_MVCC[@]}")
  OCC_BINARIES=("${TPCC_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${TPCC_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    if [[ $cmd != *"-w"* ]]
    then
      cmd="$cmd -w$i"
    fi
  }
}

setup_tpcc_noncumu_factors_mvcc() {
  EXPERIMENT_NAME="TPC-C Non-cumulative Factors (MVCC-only)"

  TPCC_OCC=(
  )

  TPCC_MVCC=(
    "MVCC (W1)" "-imvcc -g -w1"
    "MVCC (W4)" "-imvcc -g -w4"
    "MVCC (W0)" "-imvcc -g"
  )

  TPCC_OCC_BINARIES=(
  )
  TPCC_MVCC_BINARIES=(
  )
  TPCC_BOTH_BINARIES=(
    "tpcc_bench" "-f1" "NDEBUG=1 INLINED_VERSIONS=1 USE_LIBCMALLOC=1" "-AL"
    "tpcc_bench" "-f2" "NDEBUG=1 INLINED_VERSIONS=1 USE_EXCEPTION=1"  "-NOEXC"
    "tpcc_bench" "-f3" "NDEBUG=1 INLINED_VERSIONS=1 CONTENTION_REG=0" "-BACKOFF"
    "tpcc_bench" "-f4" "NDEBUG=1 INLINED_VERSIONS=1 USE_HASH_INDEX=0" "-HASH"
    "tpcc_bench" "-f0" "NDEBUG=1 INLINED_VERSIONS=1"                  "BASE"
  )

  OCC_LABELS=("${TPCC_OCC[@]}")
  MVCC_LABELS=("${TPCC_MVCC[@]}")
  OCC_BINARIES=("${TPCC_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${TPCC_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    if [[ $cmd != *"-w"* ]]
    then
      cmd="$cmd -w$i"
    fi
  }
}

setup_tpcc_mvcc_cu() {
  EXPERIMENT_NAME="TPC-C CU read at present vs past (integrated TS)."

  TPCC_OCC=(
  )

  TPCC_MVCC=(
    "MVCC (W1)" "-imvcc -g -x -w1 -r1000"
    "MVCC (W4)" "-imvcc -g -x -w4 -r1000"
    "MVCC (W0)" "-imvcc -g -x -r1000"
  )

  TPCC_OCC_BINARIES=(
  )
  TPCC_MVCC_BINARIES=(
    "tpcc_bench" "-ts-past" "NDEBUG=1 FINE_GRAINED=1 INLINED_VERSIONS=1 CU_READ_AT_PRESENT=0" " + SV + PAST"
  )
  TPCC_BOTH_BINARIES=(
    "tpcc_bench" "-past" "NDEBUG=1 INLINED_VERSIONS=1 CU_READ_AT_PRESENT=0" " + PAST"
  )

  OCC_LABELS=()
  MVCC_LABELS=("${TPCC_MVCC[@]}")
  OCC_BINARIES=()
  MVCC_BINARIES=("${TPCC_MVCC_BINARIES[@]}" "${TPCC_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    if [[ $cmd != *"-w"* ]]
    then
      cmd="$cmd -w$i"
    fi
  }
}

setup_tpcc_stacked_factors() {
  EXPERIMENT_NAME="TPC-C Stacked Factors - All"

  TPCC_OCC=(
    "OCC (W1)"          "-idefault -g -w1"
    "OCC (W4)"          "-idefault -g -w4"
    "OCC (W0)"          "-idefault -g"
  )

  TPCC_MVCC=(
    "MVCC (W1)"          "-imvcc -g -w1"
    "MVCC (W4)"          "-imvcc -g -w4"
    "MVCC (W0)"          "-imvcc -g"
  )

  TPCC_OCC_BINARIES=(
  )
  TPCC_MVCC_BINARIES=(
  )
  TPCC_BOTH_BINARIES=(
    "tpcc_bench" "-naive" "NDEBUG=1 INLINED_VERSIONS=1 USE_HASH_INDEX=0 CONTENTION_REG=0 USE_LIBCMALLOC=1 USE_EXCEPTION=1" "NAIVE"
    "tpcc_bench" "-f1" "NDEBUG=1 INLINED_VERSIONS=1 USE_HASH_INDEX=0 CONTENTION_REG=0 USE_EXCEPTION=1" "+AL"
    "tpcc_bench" "-f2" "NDEBUG=1 INLINED_VERSIONS=1 USE_HASH_INDEX=0 CONTENTION_REG=0" "+AL+NOEXC"
    "tpcc_bench" "-f3" "NDEBUG=1 INLINED_VERSIONS=1 USE_HASH_INDEX=0" "+AL+NOEXC+BACKOFF"
    #"tpcc_bench" "-f4" "NDEBUG=1 INLINED_VERSIONS=1" "+AL+BACKOFF+NOEXC+HASH"
    "tpcc_bench" "-base"  "NDEBUG=1 INLINED_VERSIONS=1" "BASE"
  )

  OCC_LABELS=("${TPCC_OCC[@]}")
  MVCC_LABELS=("${TPCC_MVCC[@]}")
  OCC_BINARIES=("${TPCC_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${TPCC_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    if [[ $cmd != *"-w"* ]]
    then
      cmd="$cmd -w$i"
    fi
  }
}

setup_tpcc_stacked_factors_occ() {
  EXPERIMENT_NAME="TPC-C Stacked Factors (OCC-only)"

  TPCC_OCC=(
    "OCC (W1)"          "-idefault -g -w1"
    "OCC (W4)"          "-idefault -g -w4"
    "OCC (W0)"          "-idefault -g"
  )

  TPCC_MVCC=(
  )

  TPCC_OCC_BINARIES=(
  )
  TPCC_MVCC_BINARIES=(
  )
  TPCC_BOTH_BINARIES=(
    "tpcc_bench" "-naive" "NDEBUG=1 INLINED_VERSIONS=1 USE_HASH_INDEX=0 CONTENTION_REG=0 USE_LIBCMALLOC=1 USE_EXCEPTION=1" "NAIVE"
    "tpcc_bench" "-f1" "NDEBUG=1 INLINED_VERSIONS=1 USE_HASH_INDEX=0 CONTENTION_REG=0 USE_EXCEPTION=1" "+AL"
    "tpcc_bench" "-f2" "NDEBUG=1 INLINED_VERSIONS=1 USE_HASH_INDEX=0 CONTENTION_REG=0" "+AL+NOEXC"
    "tpcc_bench" "-f3" "NDEBUG=1 INLINED_VERSIONS=1 USE_HASH_INDEX=0" "+AL+NOEXC+BACKOFF"
    #"tpcc_bench" "-f4" "NDEBUG=1 INLINED_VERSIONS=1" "+AL+BACKOFF+NOEXC+HASH"
    "tpcc_bench" "-base"  "NDEBUG=1 INLINED_VERSIONS=1" "BASE"
  )

  OCC_LABELS=("${TPCC_OCC[@]}")
  MVCC_LABELS=("${TPCC_MVCC[@]}")
  OCC_BINARIES=("${TPCC_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${TPCC_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    if [[ $cmd != *"-w"* ]]
    then
      cmd="$cmd -w$i"
    fi
  }
}

setup_tpcc_stacked_factors_mvcc() {
  EXPERIMENT_NAME="TPC-C Stacked Factors (MVCC-only)"

  TPCC_OCC=(
  )

  TPCC_MVCC=(
    "MVCC (W1)"          "-imvcc -g -w1"
    "MVCC (W4)"          "-imvcc -g -w4"
    "MVCC (W0)"          "-imvcc -g"
  )

  TPCC_OCC_BINARIES=(
  )
  TPCC_MVCC_BINARIES=(
  )
  TPCC_BOTH_BINARIES=(
    "tpcc_bench" "-naive" "NDEBUG=1 INLINED_VERSIONS=1 USE_HASH_INDEX=0 CONTENTION_REG=0 USE_LIBCMALLOC=1 USE_EXCEPTION=1" "NAIVE"
    "tpcc_bench" "-f1" "NDEBUG=1 INLINED_VERSIONS=1 USE_HASH_INDEX=0 CONTENTION_REG=0 USE_EXCEPTION=1" "+AL"
    "tpcc_bench" "-f2" "NDEBUG=1 INLINED_VERSIONS=1 USE_HASH_INDEX=0 CONTENTION_REG=0" "+AL+NOEXC"
    "tpcc_bench" "-f3" "NDEBUG=1 INLINED_VERSIONS=1 USE_HASH_INDEX=0" "+AL+NOEXC+BACKOFF"
    #"tpcc_bench" "-f4" "NDEBUG=1 INLINED_VERSIONS=1" "+AL+BACKOFF+NOEXC+HASH"
    "tpcc_bench" "-base"  "NDEBUG=1 INLINED_VERSIONS=1" "BASE"
  )

  OCC_LABELS=("${TPCC_OCC[@]}")
  MVCC_LABELS=("${TPCC_MVCC[@]}")
  OCC_BINARIES=("${TPCC_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${TPCC_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    if [[ $cmd != *"-w"* ]]
    then
      cmd="$cmd -w$i"
    fi
  }
}

setup_tpcc_occ_idx_cont() {
  EXPERIMENT_NAME="TPC-C Contention-aware Indexing"

  TPCC_OCC=(
    "OCC (W1)" "-idefault -g -w1 -r1000"
    "OCC (W4)" "-idefault -g -w4 -r1000"
    "OCC (W0)" "-idefault -g -r1000"
  )

  TPCC_MVCC=(
  )

  TPCC_OCC_BINARIES=(
    "tpcc_bench" "-cont-index" "PROFILE_COUNTERS=2 NDEBUG=1 CONTENTION_AWARE_IDX=0" "-CONT-AWARE-IDX"
    "tpcc_bench" "-base"       "PROFILE_COUNTERS=2 NDEBUG=1" "BASE"
  )
  TPCC_MVCC_BINARIES=(
  )
  TPCC_BOTH_BINARIES=(
  )

  OCC_LABELS=("${TPCC_OCC[@]}")
  MVCC_LABELS=()
  OCC_BINARIES=("${TPCC_OCC_BINARIES[@]}")
  MVCC_BINARIES=()

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    if [[ $cmd != *"-w"* ]]
    then
      cmd="$cmd -w$i"
    fi
  }
}

setup_tpcc_idx_cont() {
  EXPERIMENT_NAME="TPC-C Contention-aware Indexing"

  TPCC_OCC=(
    "OCC (W1)" "-idefault -g -w1 -r1000"
    "OCC (W4)" "-idefault -g -w4 -r1000"
    "OCC (W0)" "-idefault -g -r1000"
    "TicToc (W1)"      "-itictoc -g -w1 -r1000"
    "TicToc (W4)"      "-itictoc -g -w4 -r1000"
    "TicToc (W0)"      "-itictoc -g -r1000"
  )

  TPCC_MVCC=(
    "MVCC (W1)" "-imvcc -g -w1 -r1000"
    "MVCC (W4)" "-imvcc -g -w4 -r1000"
    "MVCC (W0)" "-imvcc -g -r1000"
  )

  TPCC_OCC_BINARIES=(
  )
  TPCC_MVCC_BINARIES=(
  )
  TPCC_BOTH_BINARIES=(
    "tpcc_bench" "-cont-index" "PROFILE_COUNTERS=2 NDEBUG=1 INLINED_VERSIONS=1 CONTENTION_AWARE_IDX=0" "-CONT-AWARE-IDX"
    "tpcc_bench" "-base"       "PROFILE_COUNTERS=2 NDEBUG=1 INLINED_VERSIONS=1" "BASE"
  )

  OCC_LABELS=("${TPCC_OCC[@]}")
  MVCC_LABELS=("${TPCC_MVCC[@]}")
  OCC_BINARIES=("${TPCC_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${TPCC_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    if [[ $cmd != *"-w"* ]]
    then
      cmd="$cmd -w$i"
    fi
  }
}

setup_wiki() {
  EXPERIMENT_NAME="Wikipedia"

  WIKI_OCC=(
    "OCC"         "-idefault -b"
    "OCC + CU"    "-idefault -b -x"
  )

  WIKI_MVCC=(
    "MVCC"        "-imvcc -b"
    "MVCC + CU"   "-imvcc -b -x"
  )

  WIKI_OCC_BINARIES=(
    "wiki_bench" "-occ" "NDEBUG=1 FINE_GRAINED=1" " + SV"
  )
  WIKI_MVCC_BINARIES=(
    "wiki_bench" "-mvcc" "NDEBUG=1 FINE_GRAINED=1 INLINED_VERSIONS=1" " + SV"
  )
  WIKI_BOTH_BINARIES=(
    "wiki_bench" "-both" "NDEBUG=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=("${WIKI_OCC[@]}")
  MVCC_LABELS=("${WIKI_MVCC[@]}")
  OCC_BINARIES=("${WIKI_OCC_BINARIES[@]}" "${WIKI_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${WIKI_MVCC_BINARIES[@]}" "${WIKI_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_wiki_occ() {
  EXPERIMENT_NAME="Wikipedia (OCC only)"

  WIKI_OCC=(
    "OCC"         "-idefault -b"
    "OCC + CU"    "-idefault -b -x"
  )

  WIKI_MVCC=(
  )

  WIKI_OCC_BINARIES=(
    "wiki_bench" "-occ" "NDEBUG=1 FINE_GRAINED=1" " + SV"
  )
  WIKI_MVCC_BINARIES=(
  )
  WIKI_BOTH_BINARIES=(
    "wiki_bench" "-both" "NDEBUG=1" ""
  )

  OCC_LABELS=("${WIKI_OCC[@]}")
  MVCC_LABELS=("${WIKI_MVCC[@]}")
  OCC_BINARIES=("${WIKI_OCC_BINARIES[@]}" "${WIKI_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${WIKI_MVCC_BINARIES[@]}" "${WIKI_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_wiki_mvcc() {
  EXPERIMENT_NAME="Wikipedia (MVCC only, integrated TS)"

  WIKI_OCC=(
  )

  WIKI_MVCC=(
    "MVCC"        "-imvcc -b"
    "MVCC + CU"   "-imvcc -b -x"
  )

  WIKI_OCC_BINARIES=(
  )
  WIKI_MVCC_BINARIES=(
    "wiki_bench" "-mvcc" "NDEBUG=1 FINE_GRAINED=1 INLINED_VERSIONS=1" " + SV"
  )
  WIKI_BOTH_BINARIES=(
    "wiki_bench" "-both" "NDEBUG=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=("${WIKI_OCC[@]}")
  MVCC_LABELS=("${WIKI_MVCC[@]}")
  OCC_BINARIES=("${WIKI_OCC_BINARIES[@]}" "${WIKI_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${WIKI_MVCC_BINARIES[@]}" "${WIKI_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_wiki_tictoc() {
  EXPERIMENT_NAME="Wikipedia, TicToc"

  WIKI_OCC=(
    "TicToc"      "-itictoc -b"
    "TicToc + CU" "-itictoc -b -x"
  )

  WIKI_MVCC=(
  )

  WIKI_OCC_BINARIES=(
    "wiki_bench" "-occ" "NDEBUG=1 FINE_GRAINED=1" " + SV"
  )
  WIKI_MVCC_BINARIES=(
  )
  WIKI_BOTH_BINARIES=(
    "wiki_bench" "-both" "NDEBUG=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=("${WIKI_OCC[@]}")
  MVCC_LABELS=("${WIKI_MVCC[@]}")
  OCC_BINARIES=("${WIKI_OCC_BINARIES[@]}" "${WIKI_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${WIKI_MVCC_BINARIES[@]}" "${WIKI_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_ycsba() {
  EXPERIMENT_NAME="YCSB-A"
  TIMEOUT=60

  YCSB_OCC=(
    "OCC (A)"         "-mA -idefault -g"
    "OCC (A) + CU"    "-mA -idefault -g -x"
  )

  YCSB_MVCC=(
    "MVCC (A)"        "-mA -imvcc -g"
    "MVCC (A) + CU"   "-mA -imvcc -g -x"
  )

  YCSB_OCC_BINARIES=(
    "ycsb_bench" "-occ" "NDEBUG=1 FINE_GRAINED=1" " + SV"
  )
  YCSB_MVCC_BINARIES=(
    "ycsb_bench" "-mvcc" "NDEBUG=1 FINE_GRAINED=1 INLINED_VERSIONS=1" " + SV"
  )
  YCSB_BOTH_BINARIES=(
    "ycsb_bench" "-both" "NDEBUG=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=("${YCSB_OCC[@]}")
  MVCC_LABELS=("${YCSB_MVCC[@]}")
  OCC_BINARIES=("${YCSB_OCC_BINARIES[@]}" "${YCSB_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${YCSB_MVCC_BINARIES[@]}" "${YCSB_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_ycsba_occ() {
  EXPERIMENT_NAME="YCSB-A, OCC only"
  TIMEOUT=60

  YCSB_OCC=(
    "OCC (A)"         "-mA -idefault -g"
    "OCC (A) + CU"    "-mA -idefault -g -x"
  )

  YCSB_MVCC=(
  )

  YCSB_OCC_BINARIES=(
    "ycsb_bench" "-occ" "NDEBUG=1 FINE_GRAINED=1" " + SV"
  )
  YCSB_MVCC_BINARIES=(
  )
  YCSB_BOTH_BINARIES=(
    "ycsb_bench" "-both" "NDEBUG=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=("${YCSB_OCC[@]}")
  MVCC_LABELS=()
  OCC_BINARIES=("${YCSB_OCC_BINARIES[@]}" "${YCSB_BOTH_BINARIES[@]}")
  MVCC_BINARIES=()

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_ycsba_baselines() {
  EXPERIMENT_NAME="YCSB-A, OCC vs TTCC vs MVCC"
  TIMEOUT=60

  YCSB_OCC=(
    "OCC (A)"    "-mA -idefault -g"
    "TicToc (A)" "-mA -itictoc -g"
  )

  YCSB_MVCC=(
    "MVCC (A)"   "-mA -imvcc -g"
  )

  YCSB_OCC_BINARIES=(
  )
  YCSB_MVCC_BINARIES=(
  )
  YCSB_BOTH_BINARIES=(
    "ycsb_bench" "-both" "NDEBUG=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=("${YCSB_OCC[@]}")
  MVCC_LABELS=("${YCSB_MVCC[@]}")
  OCC_BINARIES=("${YCSB_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${YCSB_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_ycsba_semopts() {
  EXPERIMENT_NAME="YCSB-A Semantic optimizations, OCC vs TTCC vs MVCC"
  TIMEOUT=60

  YCSB_OCC=(
    "OCC + CU (A)"    "-mA -idefault -g -x"
    "TicToc + CU (A)" "-mA -itictoc -g -x"
    "OCC (A)"         "-mA -idefault -g"
    "TicToc (A)"      "-mA -itictoc -g"
  )

  YCSB_MVCC=(
    "MVCC + CU (A)" "-mA -imvcc -g -x"
    "MVCC (A)"      "-mA -imvcc -g"
  )

  YCSB_OCC_BINARIES=(
    "ycsb_bench" "-occ" "NDEBUG=1 FINE_GRAINED=1" " + SV"
  )
  YCSB_MVCC_BINARIES=(
    "ycsb_bench" "-mvcc" "NDEBUG=1 FINE_GRAINED=1 INLINED_VERSIONS=1" " + SV"
  )
  YCSB_BOTH_BINARIES=(
    "ycsb_bench" "-both" "NDEBUG=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=("${YCSB_OCC[@]}")
  MVCC_LABELS=("${YCSB_MVCC[@]}")
  OCC_BINARIES=("${YCSB_OCC_BINARIES[@]}" "${YCSB_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${YCSB_MVCC_BINARIES[@]}" "${YCSB_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_ycsbb_baselines() {
  EXPERIMENT_NAME="YCSB-B, OCC vs TTCC vs MVCC"
  TIMEOUT=60

  YCSB_OCC=(
    "OCC (B)"    "-mB -idefault -g"
    "TicToc (B)" "-mB -itictoc -g"
  )

  YCSB_MVCC=(
    "MVCC (B)"   "-mB -imvcc -g"
  )

  YCSB_OCC_BINARIES=(
  )
  YCSB_MVCC_BINARIES=(
  )
  YCSB_BOTH_BINARIES=(
    "ycsb_bench" "-both" "NDEBUG=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=("${YCSB_OCC[@]}")
  MVCC_LABELS=("${YCSB_MVCC[@]}")
  OCC_BINARIES=("${YCSB_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${YCSB_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_ycsba_tictoc() {
  EXPERIMENT_NAME="YCSB-A, TicToc only"
  TIMEOUT=60

  YCSB_OCC=(
    "TicToc (A)"    "-mA -itictoc -g"
    "TicToc + CU (A)" "-mA -itictoc -g -x"
  )

  YCSB_MVCC=(
  )

  YCSB_OCC_BINARIES=(
    "ycsb_bench" "-occ" "NDEBUG=1 FINE_GRAINED=1" " + SV"
  )
  YCSB_MVCC_BINARIES=(
  )
  YCSB_BOTH_BINARIES=(
    "ycsb_bench" "-both" "NDEBUG=1" ""
  )

  OCC_LABELS=("${YCSB_OCC[@]}")
  MVCC_LABELS=()
  OCC_BINARIES=("${YCSB_OCC_BINARIES[@]}" "${YCSB_BOTH_BINARIES[@]}")
  MVCC_BINARIES=()

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_ycsba_mvcc() {
  EXPERIMENT_NAME="YCSB-A, MVCC only, integrated TS"
  TIMEOUT=60

  YCSB_OCC=(
  )

  YCSB_MVCC=(
    "MVCC (A)"        "-mA -imvcc -g"
    "MVCC (A) + CU"   "-mA -imvcc -g -x"
  )

  YCSB_OCC_BINARIES=(
  )
  YCSB_MVCC_BINARIES=(
    "ycsb_bench" "-mvcc" "NDEBUG=1 FINE_GRAINED=1 INLINED_VERSIONS=1" " + SV"
  )
  YCSB_BOTH_BINARIES=(
    "ycsb_bench" "-both" "NDEBUG=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=()
  MVCC_LABELS=("${YCSB_MVCC[@]}")
  OCC_BINARIES=()
  MVCC_BINARIES=("${YCSB_MVCC_BINARIES[@]}" "${YCSB_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_ycsbb() {
  EXPERIMENT_NAME="YCSB-B"
  TIMEOUT=60

  YCSB_OCC=(
    "OCC (B)"         "-mB -idefault -g"
    "OCC (B) + CU"    "-mB -idefault -g -x"
  )

  YCSB_MVCC=(
    "MVCC (B)"        "-mB -imvcc -g"
    "MVCC (B) + CU"   "-mB -imvcc -g -x"
  )

  YCSB_OCC_BINARIES=(
    "ycsb_bench" "-occ" "NDEBUG=1 FINE_GRAINED=1" " + SV"
  )
  YCSB_MVCC_BINARIES=(
    "ycsb_bench" "-mvcc" "NDEBUG=1 FINE_GRAINED=1 INLINED_VERSIONS=1" " + SV"
  )
  YCSB_BOTH_BINARIES=(
    "ycsb_bench" "-both" "NDEBUG=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=("${YCSB_OCC[@]}")
  MVCC_LABELS=("${YCSB_MVCC[@]}")
  OCC_BINARIES=("${YCSB_OCC_BINARIES[@]}" "${YCSB_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${YCSB_MVCC_BINARIES[@]}" "${YCSB_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_ycsbb_occ() {
  EXPERIMENT_NAME="YCSB-B, OCC only"
  TIMEOUT=60

  YCSB_OCC=(
    "OCC (B)"         "-mB -idefault -g"
    "OCC (B) + CU"    "-mB -idefault -g -x"
  )

  YCSB_MVCC=(
  )

  YCSB_OCC_BINARIES=(
    "ycsb_bench" "-occ" "NDEBUG=1 FINE_GRAINED=1" " + SV"
  )
  YCSB_MVCC_BINARIES=(
  )
  YCSB_BOTH_BINARIES=(
    "ycsb_bench" "-both" "NDEBUG=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=("${YCSB_OCC[@]}")
  MVCC_LABELS=()
  OCC_BINARIES=("${YCSB_OCC_BINARIES[@]}" "${YCSB_BOTH_BINARIES[@]}")
  MVCC_BINARIES=()

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_ycsbb_semopts() {
  EXPERIMENT_NAME="YCSB-B Semantic optimizations, OCC vs TTCC vs MVCC"
  TIMEOUT=60

  YCSB_OCC=(
    "OCC + CU (B)"    "-mB -idefault -g -x"
    "TicToc + CU (B)" "-mB -itictoc -g -x"
    "OCC (B)"         "-mB -idefault -g"
    "TicToc (B)"      "-mB -itictoc -g"
  )

  YCSB_MVCC=(
    "MVCC + CU (B)" "-mB -imvcc -g -x"
    "MVCC (B)"      "-mB -imvcc -g"
  )

  YCSB_OCC_BINARIES=(
    "ycsb_bench" "-occ" "NDEBUG=1 FINE_GRAINED=1" " + SV"
  )
  YCSB_MVCC_BINARIES=(
    "ycsb_bench" "-mvcc" "NDEBUG=1 FINE_GRAINED=1 INLINED_VERSIONS=1" " + SV"
  )
  YCSB_BOTH_BINARIES=(
    "ycsb_bench" "-both" "NDEBUG=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=("${YCSB_OCC[@]}")
  MVCC_LABELS=("${YCSB_MVCC[@]}")
  OCC_BINARIES=("${YCSB_OCC_BINARIES[@]}" "${YCSB_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${YCSB_MVCC_BINARIES[@]}" "${YCSB_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_ycsbb_tictoc() {
  EXPERIMENT_NAME="YCSB-B, TicToc only"
  TIMEOUT=60

  YCSB_OCC=(
    "TicToc (B)"    "-mB -itictoc -g"
    "TicToc + CU (B)" "-mB -itictoc -g -x"
  )

  YCSB_MVCC=(
  )

  YCSB_OCC_BINARIES=(
    "ycsb_bench" "-occ" "NDEBUG=1 FINE_GRAINED=1" " + SV"
  )
  YCSB_MVCC_BINARIES=(
  )
  YCSB_BOTH_BINARIES=(
    "ycsb_bench" "-both" "NDEBUG=1" ""
  )

  OCC_LABELS=("${YCSB_OCC[@]}")
  MVCC_LABELS=()
  OCC_BINARIES=("${YCSB_OCC_BINARIES[@]}" "${YCSB_BOTH_BINARIES[@]}")
  MVCC_BINARIES=()

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_ycsbb_mvcc() {
  EXPERIMENT_NAME="YCSB-B, MVCC only, integrated TS"
  TIMEOUT=60

  YCSB_OCC=(
  )

  YCSB_MVCC=(
    "MVCC (B)"        "-mB -imvcc -g"
    "MVCC (B) + CU"   "-mB -imvcc -g -x"
  )

  YCSB_OCC_BINARIES=(
  )
  YCSB_MVCC_BINARIES=(
    "ycsb_bench" "-mvcc" "NDEBUG=1 FINE_GRAINED=1 INLINED_VERSIONS=1" " + SV"
  )
  YCSB_BOTH_BINARIES=(
    "ycsb_bench" "-both" "NDEBUG=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=()
  MVCC_LABELS=("${YCSB_MVCC[@]}")
  OCC_BINARIES=()
  MVCC_BINARIES=("${YCSB_MVCC_BINARIES[@]}" "${YCSB_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_ycsbc() {
  EXPERIMENT_NAME="YCSB-C"
  TIMEOUT=60

  YCSB_OCC=(
    "OCC (C)"         "-mC -idefault -g"
    "OCC (C) + CU"    "-mC -idefault -g -x"
  )

  YCSB_MVCC=(
    "MVCC (C)"        "-mC -imvcc -g"
    "MVCC (C) + CU"   "-mC -imvcc -g -x"
  )

  YCSB_OCC_BINARIES=(
    "ycsb_bench" "-occ" "NDEBUG=1 FINE_GRAINED=1" " + SV"
  )
  YCSB_MVCC_BINARIES=(
    "ycsb_bench" "-mvcc" "NDEBUG=1 FINE_GRAINED=1 INLINED_VERSIONS=1" " + SV"
  )
  YCSB_BOTH_BINARIES=(
    "ycsb_bench" "-both" "NDEBUG=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=("${YCSB_OCC[@]}")
  MVCC_LABELS=("${YCSB_MVCC[@]}")
  OCC_BINARIES=("${YCSB_OCC_BINARIES[@]}" "${YCSB_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${YCSB_MVCC_BINARIES[@]}" "${YCSB_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_ycsbc_semopts() {
  EXPERIMENT_NAME="YCSB-C Semantic optimizations, OCC vs TTCC vs MVCC"
  TIMEOUT=60

  YCSB_OCC=(
    "OCC + CU (C)"    "-mC -idefault -g -x"
    "TicToc + CU (C)" "-mC -itictoc -g -x"
    "OCC (C)"         "-mC -idefault -g"
    "TicToc (C)"      "-mC -itictoc -g"
  )

  YCSB_MVCC=(
    "MVCC + CU (C)" "-mC -imvcc -g -x"
    "MVCC (C)"      "-mC -imvcc -g"
  )

  YCSB_OCC_BINARIES=(
    "ycsb_bench" "-occ" "NDEBUG=1 FINE_GRAINED=1" " + SV"
  )
  YCSB_MVCC_BINARIES=(
    "ycsb_bench" "-mvcc" "NDEBUG=1 FINE_GRAINED=1 INLINED_VERSIONS=1" " + SV"
  )
  YCSB_BOTH_BINARIES=(
    "ycsb_bench" "-both" "NDEBUG=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=("${YCSB_OCC[@]}")
  MVCC_LABELS=("${YCSB_MVCC[@]}")
  OCC_BINARIES=("${YCSB_OCC_BINARIES[@]}" "${YCSB_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${YCSB_MVCC_BINARIES[@]}" "${YCSB_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_ycsbx_semopts() {
  EXPERIMENT_NAME="YCSB-X Semantic optimizations, OCC vs TTCC vs MVCC"
  TIMEOUT=60

  YCSB_OCC=(
    "OCC + CU (X)"    "-mX -idefault -g -x"
    "TicToc + CU (X)" "-mX -itictoc -g -x"
    "OCC (X)"         "-mX -idefault -g"
    "TicToc (X)"      "-mX -itictoc -g"
  )

  YCSB_MVCC=(
    "MVCC + CU (X)" "-mX -imvcc -g -x"
    "MVCC (X)"      "-mX -imvcc -g"
  )

  YCSB_OCC_BINARIES=(
    "ycsb_bench" "-occ" "NDEBUG=1 FINE_GRAINED=1" " + SV"
  )
  YCSB_MVCC_BINARIES=(
    "ycsb_bench" "-mvcc" "NDEBUG=1 FINE_GRAINED=1 INLINED_VERSIONS=1" " + SV"
  )
  YCSB_BOTH_BINARIES=(
    "ycsb_bench" "-both" "NDEBUG=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=("${YCSB_OCC[@]}")
  MVCC_LABELS=("${YCSB_MVCC[@]}")
  OCC_BINARIES=("${YCSB_OCC_BINARIES[@]}" "${YCSB_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${YCSB_MVCC_BINARIES[@]}" "${YCSB_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_ycsby_semopts() {
  EXPERIMENT_NAME="YCSB-Y Semantic optimizations, OCC vs TTCC vs MVCC"
  TIMEOUT=60

  YCSB_OCC=(
    "OCC + CU (Y)"    "-mY -idefault -g -x"
    "TicToc + CU (Y)" "-mY -itictoc -g -x"
    "OCC (Y)"         "-mY -idefault -g"
    "TicToc (Y)"      "-mY -itictoc -g"
  )

  YCSB_MVCC=(
    "MVCC + CU (Y)" "-mY -imvcc -g -x"
    "MVCC (Y)"      "-mY -imvcc -g"
  )

  YCSB_OCC_BINARIES=(
    "ycsb_bench" "-occ" "NDEBUG=1 FINE_GRAINED=1" " + SV"
  )
  YCSB_MVCC_BINARIES=(
    "ycsb_bench" "-mvcc" "NDEBUG=1 FINE_GRAINED=1 INLINED_VERSIONS=1" " + SV"
  )
  YCSB_BOTH_BINARIES=(
    "ycsb_bench" "-both" "NDEBUG=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=("${YCSB_OCC[@]}")
  MVCC_LABELS=("${YCSB_MVCC[@]}")
  OCC_BINARIES=("${YCSB_OCC_BINARIES[@]}" "${YCSB_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${YCSB_MVCC_BINARIES[@]}" "${YCSB_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_ycsbz_semopts() {
  EXPERIMENT_NAME="YCSB-Z Semantic optimizations, OCC vs TTCC vs MVCC"
  TIMEOUT=60

  YCSB_OCC=(
    "OCC + CU (Z)"    "-mZ -idefault -g -x"
    "TicToc + CU (Z)" "-mZ -itictoc -g -x"
    "OCC (Z)"         "-mZ -idefault -g"
    "TicToc (Z)"      "-mZ -itictoc -g"
  )

  YCSB_MVCC=(
    "MVCC + CU (Z)" "-mZ -imvcc -g -x"
    "MVCC (Z)"      "-mZ -imvcc -g"
  )

  YCSB_OCC_BINARIES=(
    "ycsb_bench" "-occ" "NDEBUG=1 FINE_GRAINED=1" " + SV"
  )
  YCSB_MVCC_BINARIES=(
    "ycsb_bench" "-mvcc" "NDEBUG=1 FINE_GRAINED=1 INLINED_VERSIONS=1" " + SV"
  )
  YCSB_BOTH_BINARIES=(
    "ycsb_bench" "-both" "NDEBUG=1 INLINED_VERSIONS=1" ""
  )

  OCC_LABELS=("${YCSB_OCC[@]}")
  MVCC_LABELS=("${YCSB_MVCC[@]}")
  OCC_BINARIES=("${YCSB_OCC_BINARIES[@]}" "${YCSB_BOTH_BINARIES[@]}")
  MVCC_BINARIES=("${YCSB_MVCC_BINARIES[@]}" "${YCSB_BOTH_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}
