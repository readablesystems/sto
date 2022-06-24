#!/bin/bash

## Experiment setup documentation
#
# (Sorted in lexicographical order by setup function name)
#
# setup_adapting_100opt: Adapting microbenchmark (100 ops/txn)
# setup_adapting_1000opt: Adapting microbenchmark (1000 ops/txn)
# setup_like: LIKE
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

setup_adapting_100opt() {
  EXPERIMENT_NAME="Adapting (100 ops/txn)"
  ITERS=5

  Adapting_OCC=(
    "OCC"                "-idefault -g -v4 -snone -o100"
    "OCC + DU"           "-idefault -g -v4 -snone -o100 -x"
    "OCC + STS"          "-idefault -g -v4 -sstatic -o100"
    "OCC + STS + DU"     "-idefault -g -v4 -sstatic -o100 -x"
    "OCC + ATS"          "-idefault -g -v4 -sadaptive -o100"
    "OCC + ATS + DU"     "-idefault -g -v4 -sadaptive -o100 -x"
    "TicToc"             "-itictoc -g -v4 -snone -o100"
    "TicToc + DU"        "-itictoc -g -v4 -snone -o100 -x"
    "TicToc + STS"       "-itictoc -g -v4 -sstatic -o100"
    "TicToc + STS + DU"  "-itictoc -g -v4 -sstatic -o100 -x"
    "TicToc + ATS"       "-itictoc -g -v4 -sadaptive -o100"
    "TicToc + ATS + DU"  "-itictoc -g -v4 -sadaptive -o100 -x"
  )

  Adapting_MVCC=(
    "MVCC"               "-imvcc -g -v4 -snone -o100"
    "MVCC + DU"          "-imvcc -g -v4 -snone -o100 -x"
    "MVCC + STS"         "-imvcc -g -v4 -sstatic -o100"
    "MVCC + STS + DU"    "-imvcc -g -v4 -sstatic -o100 -x"
    "MVCC + ATS"         "-imvcc -g -v4 -sadaptive -o100"
    "MVCC + ATS + DU"    "-imvcc -g -v4 -sadaptive -o100 -x"
  )

  OCC_LABELS=("${Adapting_OCC[@]}")
  MVCC_LABELS=("${Adapting_MVCC[@]}")
  OCC_BINARIES=("adapting_bench" "" "NDEBUG=1 INLINED_VERSIONS=1" "")
  MVCC_BINARIES=("${OCC_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_adapting_1000opt() {
  EXPERIMENT_NAME="Adapting (1000 ops/txn)"
  ITERS=5

  Adapting_OCC=(
    "OCC"                "-idefault -g -v4 -snone -o1000"
    "OCC + DU"           "-idefault -g -v4 -snone -o1000 -x"
    "OCC + STS"          "-idefault -g -v4 -sstatic -o1000"
    "OCC + STS + DU"     "-idefault -g -v4 -sstatic -o1000 -x"
    "OCC + ATS"          "-idefault -g -v4 -sadaptive -o1000"
    "OCC + ATS + DU"     "-idefault -g -v4 -sadaptive -o1000 -x"
    "TicToc"             "-itictoc -g -v4 -snone -o1000"
    "TicToc + DU"        "-itictoc -g -v4 -snone -o1000 -x"
    "TicToc + STS"       "-itictoc -g -v4 -sstatic -o1000"
    "TicToc + STS + DU"  "-itictoc -g -v4 -sstatic -o1000 -x"
    "TicToc + ATS"       "-itictoc -g -v4 -sadaptive -o1000"
    "TicToc + ATS + DU"  "-itictoc -g -v4 -sadaptive -o1000 -x"
  )

  Adapting_MVCC=(
    "MVCC"               "-imvcc -g -v4 -snone -o1000"
    "MVCC + DU"          "-imvcc -g -v4 -snone -o1000 -x"
    "MVCC + STS"         "-imvcc -g -v4 -sstatic -o1000"
    "MVCC + STS + DU"    "-imvcc -g -v4 -sstatic -o1000 -x"
    "MVCC + ATS"         "-imvcc -g -v4 -sadaptive -o1000"
    "MVCC + ATS + DU"    "-imvcc -g -v4 -sadaptive -o1000 -x"
  )

  OCC_LABELS=("${Adapting_OCC[@]}")
  MVCC_LABELS=("${Adapting_MVCC[@]}")
  OCC_BINARIES=("adapting_bench" "" "NDEBUG=1 INLINED_VERSIONS=1" "")
  MVCC_BINARIES=("${OCC_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_like() {
  EXPERIMENT_NAME="LIKE"
  ITERS=5

  LIKE_OCC=(
    "OCC"                "-idefault -g -n0 -r10 -azipf -k1.4 -snone"
    "OCC + DU"           "-idefault -g -n0 -r10 -azipf -k1.4 -snone -x"
    "OCC + STS"          "-idefault -g -n0 -r10 -azipf -k1.4 -sstatic"
    "OCC + STS + DU"     "-idefault -g -n0 -r10 -azipf -k1.4 -sstatic -x"
    "OCC + ATS"          "-idefault -g -n0 -r10 -azipf -k1.4 -sadaptive"
    "OCC + ATS + DU"     "-idefault -g -n0 -r10 -azipf -k1.4 -sadaptive -x"
    "TicToc"             "-itictoc -g -n0 -r10 -azipf -k1.4 -snone"
    "TicToc + DU"        "-itictoc -g -n0 -r10 -azipf -k1.4 -snone -x"
    "TicToc + STS"       "-itictoc -g -n0 -r10 -azipf -k1.4 -sstatic"
    "TicToc + STS + DU"  "-itictoc -g -n0 -r10 -azipf -k1.4 -sstatic -x"
    "TicToc + ATS"       "-itictoc -g -n0 -r10 -azipf -k1.4 -sadaptive"
    "TicToc + ATS + DU"  "-itictoc -g -n0 -r10 -azipf -k1.4 -sadaptive -x"
  )

  LIKE_MVCC=(
    "MVCC"               "-imvcc -g -n0 -r10 -azipf -k1.4 -snone"
    "MVCC + DU"          "-imvcc -g -n0 -r10 -azipf -k1.4 -snone -x"
    "MVCC + STS"         "-imvcc -g -n0 -r10 -azipf -k1.4 -sstatic"
    "MVCC + STS + DU"    "-imvcc -g -n0 -r10 -azipf -k1.4 -sstatic -x"
    "MVCC + ATS"         "-imvcc -g -n0 -r10 -azipf -k1.4 -sadaptive"
    "MVCC + ATS + DU"  "-imvcc -g -n0 -r10 -azipf -k1.4 -sadaptive -x"
  )

  OCC_LABELS=("${LIKE_OCC[@]}")
  MVCC_LABELS=("${LIKE_MVCC[@]}")
  OCC_BINARIES=("like_bench" "" "NDEBUG=1 INLINED_VERSIONS=1" "")
  MVCC_BINARIES=("${OCC_BINARIES[@]}")

  call_runs() {
    default_call_runs
  }

  update_cmd() {
    ``  # noop
  }
}

setup_rubis() {
  EXPERIMENT_NAME="RUBiS"
  ITERS=10

  RUBIS_OCC=(
    "OCC"               "-idefault -s1.0 -g -Snone"
    "OCC + CU"          "-idefault -s1.0 -g -x -Snone"
    "OCC + STS"         "-idefault -s1.0 -g -Sstatic"
    "OCC + STS + CU"    "-idefault -s1.0 -g -x -Sstatic"
    "OCC + ATS"         "-idefault -s1.0 -g -Sadaptive"
    "OCC + ATS + CU"    "-idefault -s1.0 -g -x -Sadaptive"
    "TicToc"            "-itictoc -s1.0 -g -Snone"
    "TicToc + CU"       "-itictoc -s1.0 -g -x -Snone"
    "TicToc + STS"      "-itictoc -s1.0 -g -Sstatic"
    "TicToc + STS + CU" "-itictoc -s1.0 -g -x -Sstatic"
    "TicToc + ATS"      "-itictoc -s1.0 -g -Sadaptive"
    "TicToc + ATS + CU" "-itictoc -s1.0 -g -x -Sadaptive"
  )

  RUBIS_MVCC=(
    "MVCC"              "-imvcc -s1.0 -g -Snone"
    "MVCC + CU"         "-imvcc -s1.0 -g -x -Snone"
    "MVCC + STS"        "-imvcc -s1.0 -g -Sstatic"
    "MVCC + STS + CU"   "-imvcc -s1.0 -g -x -Sstatic"
    "MVCC + ATS"        "-imvcc -s1.0 -g -Sadaptive"
    "MVCC + ATS + CU"   "-imvcc -s1.0 -g -x -Sadaptive"
  )

  OCC_LABELS=("${RUBIS_OCC[@]}")
  MVCC_LABELS=("${RUBIS_MVCC[@]}")
  OCC_BINARIES=("rubis_bench" "" "NDEBUG=1 INLINED_VERSIONS=1" "")
  MVCC_BINARIES=("${OCC_BINARIES[@]}")

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
    "OCC"               "-idefault -s1.0 -g -Snone"
    "OCC + CU"          "-idefault -s1.0 -g -x -Snone"
    "OCC + STS"         "-idefault -s1.0 -g -Sstatic"
    "OCC + STS + CU"    "-idefault -s1.0 -g -x -Sstatic"
    "OCC + ATS"         "-idefault -s1.0 -g -Sadaptive"
    "OCC + ATS + CU"    "-idefault -s1.0 -g -x -Sadaptive"
  )

  RUBIS_MVCC=(
  )

  OCC_LABELS=("${RUBIS_OCC[@]}")
  MVCC_LABELS=("${RUBIS_MVCC[@]}")
  OCC_BINARIES=("rubis_bench" "" "NDEBUG=1 INLINED_VERSIONS=1" "")
  MVCC_BINARIES=("${OCC_BINARIES[@]}")

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
    "MVCC"              "-imvcc -s1.0 -g -Snone"
    "MVCC + CU"         "-imvcc -s1.0 -g -x -Snone"
    "MVCC + STS"        "-imvcc -s1.0 -g -Sstatic"
    "MVCC + STS + CU"   "-imvcc -s1.0 -g -x -Sstatic"
    "MVCC + ATS"        "-imvcc -s1.0 -g -Sadaptive"
    "MVCC + ATS + CU"   "-imvcc -s1.0 -g -x -Sadaptive"
  )

  OCC_LABELS=("${RUBIS_OCC[@]}")
  MVCC_LABELS=("${RUBIS_MVCC[@]}")
  OCC_BINARIES=("rubis_bench" "" "NDEBUG=1 INLINED_VERSIONS=1" "")
  MVCC_BINARIES=("${OCC_BINARIES[@]}")

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
    "TicToc"            "-itictoc -s1.0 -g -Snone"
    "TicToc + CU"       "-itictoc -s1.0 -g -x -Snone"
    "TicToc + STS"      "-itictoc -s1.0 -g -Sstatic"
    "TicToc + STS + CU" "-itictoc -s1.0 -g -x -Sstatic"
    "TicToc + ATS"      "-itictoc -s1.0 -g -Sadaptive"
    "TicToc + ATS + CU" "-itictoc -s1.0 -g -x -Sadaptive"
  )

  RUBIS_MVCC=(
  )

  OCC_LABELS=("${RUBIS_OCC[@]}")
  MVCC_LABELS=("${RUBIS_MVCC[@]}")
  OCC_BINARIES=("rubis_bench" "" "NDEBUG=1 INLINED_VERSIONS=1" "")
  MVCC_BINARIES=("${OCC_BINARIES[@]}")

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
    "OCC (W1)"         "-idefault -g -w1 -snone"
    "OCC + CU (W1)"    "-idefault -g -x -w1 -snone"
    "OCC (W4)"         "-idefault -g -w4 -snone"
    "OCC + CU (W4)"    "-idefault -g -x -w4 -snone"
    "OCC (W0)"         "-idefault -g -snone"
    "OCC + CU (W0)"    "-idefault -g -x -snone"
    "OCC + STS (W1)"         "-idefault -g -w1 -sstatic"
    "OCC + STS + CU (W1)"    "-idefault -g -x -w1 -sstatic"
    "OCC + STS (W4)"         "-idefault -g -w4 -sstatic"
    "OCC + STS + CU (W4)"    "-idefault -g -x -w4 -sstatic"
    "OCC + STS (W0)"         "-idefault -g -sstatic"
    "OCC + STS + CU (W0)"    "-idefault -g -x -sstatic"
    "OCC + ATS (W1)"         "-idefault -g -w1 -sadaptive"
    "OCC + ATS + CU (W1)"    "-idefault -g -x -w1 -sadaptive"
    "OCC + ATS (W4)"         "-idefault -g -w4 -sadaptive"
    "OCC + ATS + CU (W4)"    "-idefault -g -x -w4 -sadaptive"
    "OCC + ATS (W0)"         "-idefault -g -sadaptive"
    "OCC + ATS + CU (W0)"    "-idefault -g -x -sadaptive"
    "TicToc (W1)"         "-itictoc -g -w1 -snone"
    "TicToc + CU (W1)"    "-itictoc -g -x -w1 -snone"
    "TicToc (W4)"         "-itictoc -g -w4 -snone"
    "TicToc + CU (W4)"    "-itictoc -g -x -w4 -snone"
    "TicToc (W0)"         "-itictoc -g -snone"
    "TicToc + CU (W0)"    "-itictoc -g -x -snone"
    "TicToc + STS (W1)"         "-itictoc -g -w1 -sstatic"
    "TicToc + STS + CU (W1)"    "-itictoc -g -x -w1 -sstatic"
    "TicToc + STS (W4)"         "-itictoc -g -w4 -sstatic"
    "TicToc + STS + CU (W4)"    "-itictoc -g -x -w4 -sstatic"
    "TicToc + STS (W0)"         "-itictoc -g -sstatic"
    "TicToc + STS + CU (W0)"    "-itictoc -g -x -sstatic"
    "TicToc + ATS (W1)"         "-itictoc -g -w1 -sadaptive"
    "TicToc + ATS + CU (W1)"    "-itictoc -g -x -w1 -sadaptive"
    "TicToc + ATS (W4)"         "-itictoc -g -w4 -sadaptive"
    "TicToc + ATS + CU (W4)"    "-itictoc -g -x -w4 -sadaptive"
    "TicToc + ATS (W0)"         "-itictoc -g -sadaptive"
    "TicToc + ATS + CU (W0)"    "-itictoc -g -x -sadaptive"
  )

  TPCC_MVCC=(
    "MVCC (W1)"         "-imvcc -g -w1 -snone"
    "MVCC + CU (W1)"    "-imvcc -g -x -w1 -snone"
    "MVCC (W4)"         "-imvcc -g -w4 -snone"
    "MVCC + CU (W4)"    "-imvcc -g -x -w4 -snone"
    "MVCC (W0)"         "-imvcc -g -snone"
    "MVCC + CU (W0)"    "-imvcc -g -x -snone"
    "MVCC + STS (W1)"         "-imvcc -g -w1 -sstatic"
    "MVCC + STS + CU (W1)"    "-imvcc -g -x -w1 -sstatic"
    "MVCC + STS (W4)"         "-imvcc -g -w4 -sstatic"
    "MVCC + STS + CU (W4)"    "-imvcc -g -x -w4 -sstatic"
    "MVCC + STS (W0)"         "-imvcc -g -sstatic"
    "MVCC + STS + CU (W0)"    "-imvcc -g -x -sstatic"
    "MVCC + ATS (W1)"         "-imvcc -g -w1 -sadaptive"
    "MVCC + ATS + CU (W1)"    "-imvcc -g -x -w1 -sadaptive"
    "MVCC + ATS (W4)"         "-imvcc -g -w4 -sadaptive"
    "MVCC + ATS + CU (W4)"    "-imvcc -g -x -w4 -sadaptive"
    "MVCC + ATS (W0)"         "-imvcc -g -sadaptive"
    "MVCC + ATS + CU (W0)"    "-imvcc -g -x -sadaptive"
  )

  OCC_LABELS=("${TPCC_OCC[@]}")
  MVCC_LABELS=("${TPCC_MVCC[@]}")
  OCC_BINARIES=("tpcc_bench" "" "NDEBUG=1 OBSERVE_C_BALANCE=1 INLINED_VERSIONS=1" "")
  MVCC_BINARIES=("${OCC_BINARIES[@]}")

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
  EXPERIMENT_NAME="TPC-C MVCC"

  TPCC_OCC=(
  )

  TPCC_MVCC=(
    "MVCC (W1)"         "-imvcc -g -w1 -snone"
    "MVCC + CU (W1)"    "-imvcc -g -x -w1 -snone"
    "MVCC (W4)"         "-imvcc -g -w4 -snone"
    "MVCC + CU (W4)"    "-imvcc -g -x -w4 -snone"
    "MVCC (W0)"         "-imvcc -g -snone"
    "MVCC + CU (W0)"    "-imvcc -g -x -snone"
    "MVCC + STS (W1)"         "-imvcc -g -w1 -sstatic"
    "MVCC + STS + CU (W1)"    "-imvcc -g -x -w1 -sstatic"
    "MVCC + STS (W4)"         "-imvcc -g -w4 -sstatic"
    "MVCC + STS + CU (W4)"    "-imvcc -g -x -w4 -sstatic"
    "MVCC + STS (W0)"         "-imvcc -g -sstatic"
    "MVCC + STS + CU (W0)"    "-imvcc -g -x -sstatic"
    "MVCC + ATS (W1)"         "-imvcc -g -w1 -sadaptive"
    "MVCC + ATS + CU (W1)"    "-imvcc -g -x -w1 -sadaptive"
    "MVCC + ATS (W4)"         "-imvcc -g -w4 -sadaptive"
    "MVCC + ATS + CU (W4)"    "-imvcc -g -x -w4 -sadaptive"
    "MVCC + ATS (W0)"         "-imvcc -g -sadaptive"
    "MVCC + ATS + CU (W0)"    "-imvcc -g -x -sadaptive"
  )

  OCC_LABELS=("${TPCC_OCC[@]}")
  MVCC_LABELS=("${TPCC_MVCC[@]}")
  OCC_BINARIES=("tpcc_bench" "" "NDEBUG=1 OBSERVE_C_BALANCE=1 INLINED_VERSIONS=1" "")
  MVCC_BINARIES=("${OCC_BINARIES[@]}")

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
    "OCC (W1)"         "-idefault -g -w1 -snone"
    "OCC + CU (W1)"    "-idefault -g -x -w1 -snone"
    "OCC (W4)"         "-idefault -g -w4 -snone"
    "OCC + CU (W4)"    "-idefault -g -x -w4 -snone"
    "OCC (W0)"         "-idefault -g -snone"
    "OCC + CU (W0)"    "-idefault -g -x -snone"
    "OCC + STS (W1)"         "-idefault -g -w1 -sstatic"
    "OCC + STS + CU (W1)"    "-idefault -g -x -w1 -sstatic"
    "OCC + STS (W4)"         "-idefault -g -w4 -sstatic"
    "OCC + STS + CU (W4)"    "-idefault -g -x -w4 -sstatic"
    "OCC + STS (W0)"         "-idefault -g -sstatic"
    "OCC + STS + CU (W0)"    "-idefault -g -x -sstatic"
    "OCC + ATS (W1)"         "-idefault -g -w1 -sadaptive"
    "OCC + ATS + CU (W1)"    "-idefault -g -x -w1 -sadaptive"
    "OCC + ATS (W4)"         "-idefault -g -w4 -sadaptive"
    "OCC + ATS + CU (W4)"    "-idefault -g -x -w4 -sadaptive"
    "OCC + ATS (W0)"         "-idefault -g -sadaptive"
    "OCC + ATS + CU (W0)"    "-idefault -g -x -sadaptive"
    "TicToc (W1)"         "-itictoc -g -w1 -snone"
    "TicToc + CU (W1)"    "-itictoc -g -x -w1 -snone"
    "TicToc (W4)"         "-itictoc -g -w4 -snone"
    "TicToc + CU (W4)"    "-itictoc -g -x -w4 -snone"
    "TicToc (W0)"         "-itictoc -g -snone"
    "TicToc + CU (W0)"    "-itictoc -g -x -snone"
    "TicToc + STS (W1)"         "-itictoc -g -w1 -sstatic"
    "TicToc + STS + CU (W1)"    "-itictoc -g -x -w1 -sstatic"
    "TicToc + STS (W4)"         "-itictoc -g -w4 -sstatic"
    "TicToc + STS + CU (W4)"    "-itictoc -g -x -w4 -sstatic"
    "TicToc + STS (W0)"         "-itictoc -g -sstatic"
    "TicToc + STS + CU (W0)"    "-itictoc -g -x -sstatic"
    "TicToc + ATS (W1)"         "-itictoc -g -w1 -sadaptive"
    "TicToc + ATS + CU (W1)"    "-itictoc -g -x -w1 -sadaptive"
    "TicToc + ATS (W4)"         "-itictoc -g -w4 -sadaptive"
    "TicToc + ATS + CU (W4)"    "-itictoc -g -x -w4 -sadaptive"
    "TicToc + ATS (W0)"         "-itictoc -g -sadaptive"
    "TicToc + ATS + CU (W0)"    "-itictoc -g -x -sadaptive"
  )

  TPCC_MVCC=(
    "MVCC (W1)"         "-imvcc -g -w1 -snone"
    "MVCC + CU (W1)"    "-imvcc -g -x -w1 -snone"
    "MVCC (W4)"         "-imvcc -g -w4 -snone"
    "MVCC + CU (W4)"    "-imvcc -g -x -w4 -snone"
    "MVCC (W0)"         "-imvcc -g -snone"
    "MVCC + CU (W0)"    "-imvcc -g -x -snone"
    "MVCC + STS (W1)"         "-imvcc -g -w1 -sstatic"
    "MVCC + STS + CU (W1)"    "-imvcc -g -x -w1 -sstatic"
    "MVCC + STS (W4)"         "-imvcc -g -w4 -sstatic"
    "MVCC + STS + CU (W4)"    "-imvcc -g -x -w4 -sstatic"
    "MVCC + STS (W0)"         "-imvcc -g -sstatic"
    "MVCC + STS + CU (W0)"    "-imvcc -g -x -sstatic"
    "MVCC + ATS (W1)"         "-imvcc -g -w1 -sadaptive"
    "MVCC + ATS + CU (W1)"    "-imvcc -g -x -w1 -sadaptive"
    "MVCC + ATS (W4)"         "-imvcc -g -w4 -sadaptive"
    "MVCC + ATS + CU (W4)"    "-imvcc -g -x -w4 -sadaptive"
    "MVCC + ATS (W0)"         "-imvcc -g -sadaptive"
    "MVCC + ATS + CU (W0)"    "-imvcc -g -x -sadaptive"
  )

  OCC_LABELS=("${TPCC_OCC[@]}")
  MVCC_LABELS=("${TPCC_MVCC[@]}")
  OCC_BINARIES=("tpcc_bench" "" "NDEBUG=1 OBSERVE_C_BALANCE=1 INLINED_VERSIONS=1" "")
  MVCC_BINARIES=("${OCC_BINARIES[@]}")

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
    "TicToc (W1)"         "-itictoc -g -n -w1 -snone"
    "TicToc + CU (W1)"    "-itictoc -g -n -x -w1 -snone"
    "TicToc (W4)"         "-itictoc -g -n -w4 -snone"
    "TicToc + CU (W4)"    "-itictoc -g -n -x -w4 -snone"
    "TicToc (W0)"         "-itictoc -g -n -snone"
    "TicToc + CU (W0)"    "-itictoc -g -n -x -snone"
    "TicToc + STS (W1)"         "-itictoc -g -n -w1 -sstatic"
    "TicToc + STS + CU (W1)"    "-itictoc -g -n -x -w1 -sstatic"
    "TicToc + STS (W4)"         "-itictoc -g -n -w4 -sstatic"
    "TicToc + STS + CU (W4)"    "-itictoc -g -n -x -w4 -sstatic"
    "TicToc + STS (W0)"         "-itictoc -g -n -sstatic"
    "TicToc + STS + CU (W0)"    "-itictoc -g -n -x -sstatic"
    "TicToc + ATS (W1)"         "-itictoc -g -n -w1 -sadaptive"
    "TicToc + ATS + CU (W1)"    "-itictoc -g -n -x -w1 -sadaptive"
    "TicToc + ATS (W4)"         "-itictoc -g -n -w4 -sadaptive"
    "TicToc + ATS + CU (W4)"    "-itictoc -g -n -x -w4 -sadaptive"
    "TicToc + ATS (W0)"         "-itictoc -g -n -sadaptive"
    "TicToc + ATS + CU (W0)"    "-itictoc -g -n -x -sadaptive"
  )

  TPCC_MVCC=(
  )

  OCC_LABELS=("${TPCC_OCC[@]}")
  MVCC_LABELS=("${TPCC_MVCC[@]}")
  OCC_BINARIES=("tpcc_bench" "" "NDEBUG=1 OBSERVE_C_BALANCE=1 INLINED_VERSIONS=1" "")
  MVCC_BINARIES=("${OCC_BINARIES[@]}")

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
setup_tpcc_tictoc() {
  setup_tpcc_tictoc_full
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
    "OCC"         "-idefault -b -snone"
    "OCC + CU"    "-idefault -b -x -snone"
    "OCC + STS"         "-idefault -b -sstatic"
    "OCC + STS + CU"    "-idefault -b -x -sstatic"
    "OCC + ATS"         "-idefault -b -sadaptive"
    "OCC + ATS + CU"    "-idefault -b -x -sadaptive"
    "TicToc"         "-itictoc -b -snone"
    "TicToc + CU"    "-itictoc -b -x -snone"
    "TicToc + STS"         "-itictoc -b -sstatic"
    "TicToc + STS + CU"    "-itictoc -b -x -sstatic"
    "TicToc + ATS"         "-itictoc -b -sadaptive"
    "TicToc + ATS + CU"    "-itictoc -b -x -sadaptive"
  )

  WIKI_MVCC=(
    "MVCC"         "-imvcc -b -snone"
    "MVCC + CU"    "-imvcc -b -x -snone"
    "MVCC + STS"         "-imvcc -b -sstatic"
    "MVCC + STS + CU"    "-imvcc -b -x -sstatic"
    "MVCC + ATS"         "-imvcc -b -sadaptive"
    "MVCC + ATS + CU"    "-imvcc -b -x -sadaptive"
  )

  OCC_LABELS=("${WIKI_OCC[@]}")
  MVCC_LABELS=("${WIKI_MVCC[@]}")
  OCC_BINARIES=("wiki_bench" "" "NDEBUG=1 INLINED_VERSIONS=1" "")
  MVCC_BINARIES=("${OCC_BINARIES[@]}")

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
    "OCC"         "-idefault -b -snone"
    "OCC + CU"    "-idefault -b -x -snone"
    "OCC + STS"         "-idefault -b -sstatic"
    "OCC + STS + CU"    "-idefault -b -x -sstatic"
    "OCC + ATS"         "-idefault -b -sadaptive"
    "OCC + ATS + CU"    "-idefault -b -x -sadaptive"
  )

  WIKI_MVCC=(
  )

  OCC_LABELS=("${WIKI_OCC[@]}")
  MVCC_LABELS=("${WIKI_MVCC[@]}")
  OCC_BINARIES=("wiki_bench" "" "NDEBUG=1 INLINED_VERSIONS=1" "")
  MVCC_BINARIES=("${OCC_BINARIES[@]}")

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
    "MVCC"         "-imvcc -b -snone"
    "MVCC + CU"    "-imvcc -b -x -snone"
    "MVCC + STS"         "-imvcc -b -sstatic"
    "MVCC + STS + CU"    "-imvcc -b -x -sstatic"
    "MVCC + ATS"         "-imvcc -b -sadaptive"
    "MVCC + ATS + CU"    "-imvcc -b -x -sadaptive"
  )

  OCC_LABELS=("${WIKI_OCC[@]}")
  MVCC_LABELS=("${WIKI_MVCC[@]}")
  OCC_BINARIES=("wiki_bench" "" "NDEBUG=1 INLINED_VERSIONS=1" "")
  MVCC_BINARIES=("${OCC_BINARIES[@]}")

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
    "TicToc"         "-itictoc -b -snone"
    "TicToc + CU"    "-itictoc -b -x -snone"
    "TicToc + STS"         "-itictoc -b -sstatic"
    "TicToc + STS + CU"    "-itictoc -b -x -sstatic"
    "TicToc + ATS"         "-itictoc -b -sadaptive"
    "TicToc + ATS + CU"    "-itictoc -b -x -sadaptive"
  )

  WIKI_MVCC=(
  )

  OCC_LABELS=("${WIKI_OCC[@]}")
  MVCC_LABELS=("${WIKI_MVCC[@]}")
  OCC_BINARIES=("wiki_bench" "" "NDEBUG=1 INLINED_VERSIONS=1" "")
  MVCC_BINARIES=("${OCC_BINARIES[@]}")

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
    "OCC (A)"         "-mA -idefault -g -snone"
    "OCC (A) + CU"    "-mA -idefault -g -x -snone"
    "OCC (A) + STS"         "-mA -idefault -g -sstatic"
    "OCC (A) + STS + CU"    "-mA -idefault -g -x -sstatic"
    "OCC (A) + ATS"         "-mA -idefault -g -sadaptive"
    "OCC (A) + ATS + CU"    "-mA -idefault -g -x -sadaptive"
    "TicToc (A)"         "-mA -itictoc -g -snone"
    "TicToc (A) + CU"    "-mA -itictoc -g -x -snone"
    "TicToc (A) + STS"         "-mA -itictoc -g -sstatic"
    "TicToc (A) + STS + CU"    "-mA -itictoc -g -x -sstatic"
    "TicToc (A) + ATS"         "-mA -itictoc -g -sadaptive"
    "TicToc (A) + ATS + CU"    "-mA -itictoc -g -x -sadaptive"
  )

  YCSB_MVCC=(
    "MVCC (A)"         "-mA -imvcc -g -snone"
    "MVCC (A) + CU"    "-mA -imvcc -g -x -snone"
    "MVCC (A) + STS"         "-mA -imvcc -g -sstatic"
    "MVCC (A) + STS + CU"    "-mA -imvcc -g -x -sstatic"
    "MVCC (A) + ATS"         "-mA -imvcc -g -sadaptive"
    "MVCC (A) + ATS + CU"    "-mA -imvcc -g -x -sadaptive"
  )

  OCC_LABELS=("${YCSB_OCC[@]}")
  MVCC_LABELS=("${YCSB_MVCC[@]}")
  OCC_BINARIES=("ycsb_bench" "" "NDEBUG=1 INLINED_VERSIONS=1" "")
  MVCC_BINARIES=("${OCC_BINARIES[@]}")

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
    "OCC (A)"         "-mA -idefault -g -snone"
    "OCC + CU (A)"    "-mA -idefault -g -x -snone"
    "OCC + STS (A)"         "-mA -idefault -g -sstatic"
    "OCC + STS + CU (A)"    "-mA -idefault -g -x -sstatic"
    "OCC + ATS (A)"         "-mA -idefault -g -sadaptive"
    "OCC + ATS + CU (A)"    "-mA -idefault -g -x -sadaptive"
  )

  YCSB_MVCC=(
  )

  OCC_LABELS=("${YCSB_OCC[@]}")
  MVCC_LABELS=("${YCSB_MVCC[@]}")
  OCC_BINARIES=("ycsb_bench" "" "NDEBUG=1 INLINED_VERSIONS=1" "")
  MVCC_BINARIES=("${OCC_BINARIES[@]}")

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
    "OCC (A)"         "-mA -idefault -g -snone"
    "OCC + CU (A)"    "-mA -idefault -g -x -snone"
    "OCC + STS (A)"         "-mA -idefault -g -sstatic"
    "OCC + STS + CU (A)"    "-mA -idefault -g -x -sstatic"
    "OCC + ATS (A)"         "-mA -idefault -g -sadaptive"
    "OCC + ATS + CU (A)"    "-mA -idefault -g -x -sadaptive"
    "TicToc (A)"         "-mA -itictoc -g -snone"
    "TicToc + CU (A)"    "-mA -itictoc -g -x -snone"
    "TicToc + STS (A)"         "-mA -itictoc -g -sstatic"
    "TicToc + STS + CU (A)"    "-mA -itictoc -g -x -sstatic"
    "TicToc + ATS (A)"         "-mA -itictoc -g -sadaptive"
    "TicToc + ATS + CU (A)"    "-mA -itictoc -g -x -sadaptive"
  )

  YCSB_MVCC=(
    "MVCC (A)"         "-mA -imvcc -g -snone"
    "MVCC + CU (A)"    "-mA -imvcc -g -x -snone"
    "MVCC + STS (A)"         "-mA -imvcc -g -sstatic"
    "MVCC + STS + CU (A)"    "-mA -imvcc -g -x -sstatic"
    "MVCC + ATS (A)"         "-mA -imvcc -g -sadaptive"
    "MVCC + ATS + CU (A)"    "-mA -imvcc -g -x -sadaptive"
  )

  OCC_LABELS=("${YCSB_OCC[@]}")
  MVCC_LABELS=("${YCSB_MVCC[@]}")
  OCC_BINARIES=("ycsb_bench" "" "NDEBUG=1 INLINED_VERSIONS=1" "")
  MVCC_BINARIES=("${OCC_BINARIES[@]}")

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
    "TicToc (A)"         "-mA -itictoc -g -snone"
    "TicToc + CU (A)"    "-mA -itictoc -g -x -snone"
    "TicToc + STS (A)"         "-mA -itictoc -g -sstatic"
    "TicToc + STS + CU (A)"    "-mA -itictoc -g -x -sstatic"
    "TicToc + ATS (A)"         "-mA -itictoc -g -sadaptive"
    "TicToc + ATS + CU (A)"    "-mA -itictoc -g -x -sadaptive"
  )

  YCSB_MVCC=(
  )

  OCC_LABELS=("${YCSB_OCC[@]}")
  MVCC_LABELS=("${YCSB_MVCC[@]}")
  OCC_BINARIES=("ycsb_bench" "" "NDEBUG=1 INLINED_VERSIONS=1" "")
  MVCC_BINARIES=("${OCC_BINARIES[@]}")

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
    "MVCC (A)"         "-mA -imvcc -g -snone"
    "MVCC + CU (A)"    "-mA -imvcc -g -x -snone"
    "MVCC + STS (A)"         "-mA -imvcc -g -sstatic"
    "MVCC + STS + CU (A)"    "-mA -imvcc -g -x -sstatic"
    "MVCC + ATS (A)"         "-mA -imvcc -g -sadaptive"
    "MVCC + ATS + CU (A)"    "-mA -imvcc -g -x -sadaptive"
  )

  OCC_LABELS=("${YCSB_OCC[@]}")
  MVCC_LABELS=("${YCSB_MVCC[@]}")
  OCC_BINARIES=("ycsb_bench" "" "NDEBUG=1 INLINED_VERSIONS=1" "")
  MVCC_BINARIES=("${OCC_BINARIES[@]}")

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
    "OCC (B)"         "-mB -idefault -g -snone"
    "OCC + CU (B)"    "-mB -idefault -g -x -snone"
    "OCC + STS (B)"         "-mB -idefault -g -sstatic"
    "OCC + STS + CU (B)"    "-mB -idefault -g -x -sstatic"
    "OCC + ATS (B)"         "-mB -idefault -g -sadaptive"
    "OCC + ATS + CU (B)"    "-mB -idefault -g -x -sadaptive"
    "TicToc (B)"         "-mB -itictoc -g -snone"
    "TicToc + CU (B)"    "-mB -itictoc -g -x -snone"
    "TicToc + STS (B)"         "-mB -itictoc -g -sstatic"
    "TicToc + STS + CU (B)"    "-mB -itictoc -g -x -sstatic"
    "TicToc + ATS (B)"         "-mB -itictoc -g -sadaptive"
    "TicToc + ATS + CU (B)"    "-mB -itictoc -g -x -sadaptive"
  )

  YCSB_MVCC=(
    "MVCC (B)"         "-mB -imvcc -g -snone"
    "MVCC + CU (B)"    "-mB -imvcc -g -x -snone"
    "MVCC + STS (B)"         "-mB -imvcc -g -sstatic"
    "MVCC + STS + CU (B)"    "-mB -imvcc -g -x -sstatic"
    "MVCC + ATS (B)"         "-mB -imvcc -g -sadaptive"
    "MVCC + ATS + CU (B)"    "-mB -imvcc -g -x -sadaptive"
  )

  OCC_LABELS=("${YCSB_OCC[@]}")
  MVCC_LABELS=("${YCSB_MVCC[@]}")
  OCC_BINARIES=("ycsb_bench" "" "NDEBUG=1 INLINED_VERSIONS=1" "")
  MVCC_BINARIES=("${OCC_BINARIES[@]}")

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
    "OCC (B)"         "-mB -idefault -g -snone"
    "OCC + CU (B)"    "-mB -idefault -g -x -snone"
    "OCC + STS (B)"         "-mB -idefault -g -sstatic"
    "OCC + STS + CU (B)"    "-mB -idefault -g -x -sstatic"
    "OCC + ATS (B)"         "-mB -idefault -g -sadaptive"
    "OCC + ATS + CU (B)"    "-mB -idefault -g -x -sadaptive"
  )

  YCSB_MVCC=(
  )

  OCC_LABELS=("${YCSB_OCC[@]}")
  MVCC_LABELS=("${YCSB_MVCC[@]}")
  OCC_BINARIES=("ycsb_bench" "" "NDEBUG=1 INLINED_VERSIONS=1" "")
  MVCC_BINARIES=("${OCC_BINARIES[@]}")

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
    "OCC (B)"         "-mB -idefault -g -snone"
    "OCC + CU (B)"    "-mB -idefault -g -x -snone"
    "OCC + STS (B)"         "-mB -idefault -g -sstatic"
    "OCC + STS + CU (B)"    "-mB -idefault -g -x -sstatic"
    "OCC + ATS (B)"         "-mB -idefault -g -sadaptive"
    "OCC + ATS + CU (B)"    "-mB -idefault -g -x -sadaptive"
    "TicToc (B)"         "-mB -itictoc -g -snone"
    "TicToc + CU (B)"    "-mB -itictoc -g -x -snone"
    "TicToc + STS (B)"         "-mB -itictoc -g -sstatic"
    "TicToc + STS + CU (B)"    "-mB -itictoc -g -x -sstatic"
    "TicToc + ATS (B)"         "-mB -itictoc -g -sadaptive"
    "TicToc + ATS + CU (B)"    "-mB -itictoc -g -x -sadaptive"
  )

  YCSB_MVCC=(
    "MVCC (B)"         "-mB -imvcc -g -snone"
    "MVCC + CU (B)"    "-mB -imvcc -g -x -snone"
    "MVCC + STS (B)"         "-mB -imvcc -g -sstatic"
    "MVCC + STS + CU (B)"    "-mB -imvcc -g -x -sstatic"
    "MVCC + ATS (B)"         "-mB -imvcc -g -sadaptive"
    "MVCC + ATS + CU (B)"    "-mB -imvcc -g -x -sadaptive"
  )

  OCC_LABELS=("${YCSB_OCC[@]}")
  MVCC_LABELS=("${YCSB_MVCC[@]}")
  OCC_BINARIES=("ycsb_bench" "" "NDEBUG=1 INLINED_VERSIONS=1" "")
  MVCC_BINARIES=("${OCC_BINARIES[@]}")

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
    "TicToc (B)"         "-mB -itictoc -g -snone"
    "TicToc + CU (B)"    "-mB -itictoc -g -x -snone"
    "TicToc + STS (B)"         "-mB -itictoc -g -sstatic"
    "TicToc + STS + CU (B)"    "-mB -itictoc -g -x -sstatic"
    "TicToc + ATS (B)"         "-mB -itictoc -g -sadaptive"
    "TicToc + ATS + CU (B)"    "-mB -itictoc -g -x -sadaptive"
  )

  YCSB_MVCC=(
  )

  OCC_LABELS=("${YCSB_OCC[@]}")
  MVCC_LABELS=("${YCSB_MVCC[@]}")
  OCC_BINARIES=("ycsb_bench" "" "NDEBUG=1 INLINED_VERSIONS=1" "")
  MVCC_BINARIES=("${OCC_BINARIES[@]}")

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
    "MVCC (B)"         "-mB -imvcc -g -snone"
    "MVCC + CU (B)"    "-mB -imvcc -g -x -snone"
    "MVCC + STS (B)"         "-mB -imvcc -g -sstatic"
    "MVCC + STS + CU (B)"    "-mB -imvcc -g -x -sstatic"
    "MVCC + ATS (B)"         "-mB -imvcc -g -sadaptive"
    "MVCC + ATS + CU (B)"    "-mB -imvcc -g -x -sadaptive"
  )

  OCC_LABELS=("${YCSB_OCC[@]}")
  MVCC_LABELS=("${YCSB_MVCC[@]}")
  OCC_BINARIES=("ycsb_bench" "" "NDEBUG=1 INLINED_VERSIONS=1" "")
  MVCC_BINARIES=("${OCC_BINARIES[@]}")

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
    "OCC (C)"         "-mC -idefault -g -snone"
    "OCC + CU (C)"    "-mC -idefault -g -x -snone"
    "OCC + STS (C)"         "-mC -idefault -g -sstatic"
    "OCC + STS + CU (C)"    "-mC -idefault -g -x -sstatic"
    "OCC + ATS (C)"         "-mC -idefault -g -sadaptive"
    "OCC + ATS + CU (C)"    "-mC -idefault -g -x -sadaptive"
    "TicToc (C)"         "-mC -itictoc -g -snone"
    "TicToc + CU (C)"    "-mC -itictoc -g -x -snone"
    "TicToc + STS (C)"         "-mC -itictoc -g -sstatic"
    "TicToc + STS + CU (C)"    "-mC -itictoc -g -x -sstatic"
    "TicToc + ATS (C)"         "-mC -itictoc -g -sadaptive"
    "TicToc + ATS + CU (C)"    "-mC -itictoc -g -x -sadaptive"
  )

  YCSB_MVCC=(
    "MVCC (C)"         "-mC -imvcc -g -snone"
    "MVCC + CU (C)"    "-mC -imvcc -g -x -snone"
    "MVCC + STS (C)"         "-mC -imvcc -g -sstatic"
    "MVCC + STS + CU (C)"    "-mC -imvcc -g -x -sstatic"
    "MVCC + ATS (C)"         "-mC -imvcc -g -sadaptive"
    "MVCC + ATS + CU (C)"    "-mC -imvcc -g -x -sadaptive"
  )

  OCC_LABELS=("${YCSB_OCC[@]}")
  MVCC_LABELS=("${YCSB_MVCC[@]}")
  OCC_BINARIES=("ycsb_bench" "" "NDEBUG=1 INLINED_VERSIONS=1" "")
  MVCC_BINARIES=("${OCC_BINARIES[@]}")

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
