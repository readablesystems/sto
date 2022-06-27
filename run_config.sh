#!/bin/bash

## Experiment setup documentation
#
# (Sorted in lexicographical order by setup function name)
#
# setup_adapting_100opt: Adapting microbenchmark (100 ops/txn), 2 split policies active
# setup_adapting_1000opt: Adapting microbenchmark (1000 ops/txn), 2 split policies active
# setup_adapting_100opt_4sp: Adapting microbenchmark (100 ops/txn), 4 split policies active
# setup_adapting_1000opt_4sp: Adapting microbenchmark (1000 ops/txn), 4 split policies active
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
    "OCC (100opt-2sp)"                "-idefault -g -v2 -snone -o100"
    "OCC + DU (100opt-2sp)"           "-idefault -g -v2 -snone -o100 -x"
    "OCC + STS (100opt-2sp)"          "-idefault -g -v2 -sstatic -o100"
    "OCC + STS + DU (100opt-2sp)"     "-idefault -g -v2 -sstatic -o100 -x"
    "OCC + ATS (100opt-2sp)"          "-idefault -g -v2 -sadaptive -o100"
    "OCC + ATS + DU (100opt-2sp)"     "-idefault -g -v2 -sadaptive -o100 -x"
    "TicToc (100opt-2sp)"             "-itictoc -g -v2 -snone -o100"
    "TicToc + DU (100opt-2sp)"        "-itictoc -g -v2 -snone -o100 -x"
    "TicToc + STS (100opt-2sp)"       "-itictoc -g -v2 -sstatic -o100"
    "TicToc + STS + DU (100opt-2sp)"  "-itictoc -g -v2 -sstatic -o100 -x"
    "TicToc + ATS (100opt-2sp)"       "-itictoc -g -v2 -sadaptive -o100"
    "TicToc + ATS + DU (100opt-2sp)"  "-itictoc -g -v2 -sadaptive -o100 -x"
  )

  Adapting_MVCC=(
    "MVCC (100opt-2sp)"               "-imvcc -g -v2 -snone -o100"
    "MVCC + DU (100opt-2sp)"          "-imvcc -g -v2 -snone -o100 -x"
    "MVCC + STS (100opt-2sp)"         "-imvcc -g -v2 -sstatic -o100"
    "MVCC + STS + DU (100opt-2sp)"    "-imvcc -g -v2 -sstatic -o100 -x"
    "MVCC + ATS (100opt-2sp)"         "-imvcc -g -v2 -sadaptive -o100"
    "MVCC + ATS + DU (100opt-2sp)"    "-imvcc -g -v2 -sadaptive -o100 -x"
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
    "OCC (1000opt-2sp)"                "-idefault -g -v2 -snone -o1000"
    "OCC + DU (1000opt-2sp)"           "-idefault -g -v2 -snone -o1000 -x"
    "OCC + STS (1000opt-2sp)"          "-idefault -g -v2 -sstatic -o1000"
    "OCC + STS + DU (1000opt-2sp)"     "-idefault -g -v2 -sstatic -o1000 -x"
    "OCC + ATS (1000opt-2sp)"          "-idefault -g -v2 -sadaptive -o1000"
    "OCC + ATS + DU (1000opt-2sp)"     "-idefault -g -v2 -sadaptive -o1000 -x"
    "TicToc (1000opt-2sp)"             "-itictoc -g -v2 -snone -o1000"
    "TicToc + DU (1000opt-2sp)"        "-itictoc -g -v2 -snone -o1000 -x"
    "TicToc + STS (1000opt-2sp)"       "-itictoc -g -v2 -sstatic -o1000"
    "TicToc + STS + DU (1000opt-2sp)"  "-itictoc -g -v2 -sstatic -o1000 -x"
    "TicToc + ATS (1000opt-2sp)"       "-itictoc -g -v2 -sadaptive -o1000"
    "TicToc + ATS + DU (1000opt-2sp)"  "-itictoc -g -v2 -sadaptive -o1000 -x"
  )

  Adapting_MVCC=(
    "MVCC (1000opt-2sp)"               "-imvcc -g -v2 -snone -o1000"
    "MVCC + DU (1000opt-2sp)"          "-imvcc -g -v2 -snone -o1000 -x"
    "MVCC + STS (1000opt-2sp)"         "-imvcc -g -v2 -sstatic -o1000"
    "MVCC + STS + DU (1000opt-2sp)"    "-imvcc -g -v2 -sstatic -o1000 -x"
    "MVCC + ATS (1000opt-2sp)"         "-imvcc -g -v2 -sadaptive -o1000"
    "MVCC + ATS + DU (1000opt-2sp)"    "-imvcc -g -v2 -sadaptive -o1000 -x"
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

setup_adapting_100opt_4sp() {
  EXPERIMENT_NAME="Adapting (100 ops/txn): 4 split policies active"
  ITERS=5

  Adapting_OCC=(
    "OCC (100opt-4sp)"                "-idefault -g -v4 -snone -o100"
    "OCC + DU (100opt-4sp)"           "-idefault -g -v4 -snone -o100 -x"
    "OCC + STS (100opt-4sp)"          "-idefault -g -v4 -sstatic -o100"
    "OCC + STS + DU (100opt-4sp)"     "-idefault -g -v4 -sstatic -o100 -x"
    "OCC + ATS (100opt-4sp)"          "-idefault -g -v4 -sadaptive -o100"
    "OCC + ATS + DU (100opt-4sp)"     "-idefault -g -v4 -sadaptive -o100 -x"
    "TicToc (100opt-4sp)"             "-itictoc -g -v4 -snone -o100"
    "TicToc + DU (100opt-4sp)"        "-itictoc -g -v4 -snone -o100 -x"
    "TicToc + STS (100opt-4sp)"       "-itictoc -g -v4 -sstatic -o100"
    "TicToc + STS + DU (100opt-4sp)"  "-itictoc -g -v4 -sstatic -o100 -x"
    "TicToc + ATS (100opt-4sp)"       "-itictoc -g -v4 -sadaptive -o100"
    "TicToc + ATS + DU (100opt-4sp)"  "-itictoc -g -v4 -sadaptive -o100 -x"
  )

  Adapting_MVCC=(
    "MVCC (100opt-4sp)"               "-imvcc -g -v4 -snone -o100"
    "MVCC + DU (100opt-4sp)"          "-imvcc -g -v4 -snone -o100 -x"
    "MVCC + STS (100opt-4sp)"         "-imvcc -g -v4 -sstatic -o100"
    "MVCC + STS + DU (100opt-4sp)"    "-imvcc -g -v4 -sstatic -o100 -x"
    "MVCC + ATS (100opt-4sp)"         "-imvcc -g -v4 -sadaptive -o100"
    "MVCC + ATS + DU (100opt-4sp)"    "-imvcc -g -v4 -sadaptive -o100 -x"
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

setup_adapting_1000opt_4sp() {
  EXPERIMENT_NAME="Adapting (1000 ops/txn): 4 split policies active"
  ITERS=5

  Adapting_OCC=(
    "OCC (1000opt-4sp)"                "-idefault -g -v4 -snone -o1000"
    "OCC + DU (1000opt-4sp)"           "-idefault -g -v4 -snone -o1000 -x"
    "OCC + STS (1000opt-4sp)"          "-idefault -g -v4 -sstatic -o1000"
    "OCC + STS + DU (1000opt-4sp)"     "-idefault -g -v4 -sstatic -o1000 -x"
    "OCC + ATS (1000opt-4sp)"          "-idefault -g -v4 -sadaptive -o1000"
    "OCC + ATS + DU (1000opt-4sp)"     "-idefault -g -v4 -sadaptive -o1000 -x"
    "TicToc (1000opt-4sp)"             "-itictoc -g -v4 -snone -o1000"
    "TicToc + DU (1000opt-4sp)"        "-itictoc -g -v4 -snone -o1000 -x"
    "TicToc + STS (1000opt-4sp)"       "-itictoc -g -v4 -sstatic -o1000"
    "TicToc + STS + DU (1000opt-4sp)"  "-itictoc -g -v4 -sstatic -o1000 -x"
    "TicToc + ATS (1000opt-4sp)"       "-itictoc -g -v4 -sadaptive -o1000"
    "TicToc + ATS + DU (1000opt-4sp)"  "-itictoc -g -v4 -sadaptive -o1000 -x"
  )

  Adapting_MVCC=(
    "MVCC (1000opt-4sp)"               "-imvcc -g -v4 -snone -o1000"
    "MVCC + DU (1000opt-4sp)"          "-imvcc -g -v4 -snone -o1000 -x"
    "MVCC + STS (1000opt-4sp)"         "-imvcc -g -v4 -sstatic -o1000"
    "MVCC + STS + DU (1000opt-4sp)"    "-imvcc -g -v4 -sstatic -o1000 -x"
    "MVCC + ATS (1000opt-4sp)"         "-imvcc -g -v4 -sadaptive -o1000"
    "MVCC + ATS + DU (1000opt-4sp)"    "-imvcc -g -v4 -sadaptive -o1000 -x"
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
    "MVCC + ATS + DU"    "-imvcc -g -n0 -r10 -azipf -k1.4 -sadaptive -x"
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
    "OCC + DU"          "-idefault -s1.0 -g -x -Snone"
    "OCC + STS"         "-idefault -s1.0 -g -Sstatic"
    "OCC + STS + DU"    "-idefault -s1.0 -g -x -Sstatic"
    "OCC + ATS"         "-idefault -s1.0 -g -Sadaptive"
    "OCC + ATS + DU"    "-idefault -s1.0 -g -x -Sadaptive"
    "TicToc"            "-itictoc -s1.0 -g -Snone"
    "TicToc + DU"       "-itictoc -s1.0 -g -x -Snone"
    "TicToc + STS"      "-itictoc -s1.0 -g -Sstatic"
    "TicToc + STS + DU" "-itictoc -s1.0 -g -x -Sstatic"
    "TicToc + ATS"      "-itictoc -s1.0 -g -Sadaptive"
    "TicToc + ATS + DU" "-itictoc -s1.0 -g -x -Sadaptive"
  )

  RUBIS_MVCC=(
    "MVCC"              "-imvcc -s1.0 -g -Snone"
    "MVCC + DU"         "-imvcc -s1.0 -g -x -Snone"
    "MVCC + STS"        "-imvcc -s1.0 -g -Sstatic"
    "MVCC + STS + DU"   "-imvcc -s1.0 -g -x -Sstatic"
    "MVCC + ATS"        "-imvcc -s1.0 -g -Sadaptive"
    "MVCC + ATS + DU"   "-imvcc -s1.0 -g -x -Sadaptive"
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
    "OCC + DU"          "-idefault -s1.0 -g -x -Snone"
    "OCC + STS"         "-idefault -s1.0 -g -Sstatic"
    "OCC + STS + DU"    "-idefault -s1.0 -g -x -Sstatic"
    "OCC + ATS"         "-idefault -s1.0 -g -Sadaptive"
    "OCC + ATS + DU"    "-idefault -s1.0 -g -x -Sadaptive"
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
    "MVCC + DU"         "-imvcc -s1.0 -g -x -Snone"
    "MVCC + STS"        "-imvcc -s1.0 -g -Sstatic"
    "MVCC + STS + DU"   "-imvcc -s1.0 -g -x -Sstatic"
    "MVCC + ATS"        "-imvcc -s1.0 -g -Sadaptive"
    "MVCC + ATS + DU"   "-imvcc -s1.0 -g -x -Sadaptive"
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
    "TicToc + DU"       "-itictoc -s1.0 -g -x -Snone"
    "TicToc + STS"      "-itictoc -s1.0 -g -Sstatic"
    "TicToc + STS + DU" "-itictoc -s1.0 -g -x -Sstatic"
    "TicToc + ATS"      "-itictoc -s1.0 -g -Sadaptive"
    "TicToc + ATS + DU" "-itictoc -s1.0 -g -x -Sadaptive"
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
    "OCC (W1)"         "-idefault -g -r1000 -w1 -snone"
    "OCC + DU (W1)"    "-idefault -g -r1000 -x -w1 -snone"
    "OCC (W4)"         "-idefault -g -r1000 -w4 -snone"
    "OCC + DU (W4)"    "-idefault -g -r1000 -x -w4 -snone"
    "OCC (W0)"         "-idefault -g -r1000 -snone"
    "OCC + DU (W0)"    "-idefault -g -r1000 -x -snone"
    "OCC + STS (W1)"         "-idefault -g -r1000 -w1 -sstatic"
    "OCC + STS + DU (W1)"    "-idefault -g -r1000 -x -w1 -sstatic"
    "OCC + STS (W4)"         "-idefault -g -r1000 -w4 -sstatic"
    "OCC + STS + DU (W4)"    "-idefault -g -r1000 -x -w4 -sstatic"
    "OCC + STS (W0)"         "-idefault -g -r1000 -sstatic"
    "OCC + STS + DU (W0)"    "-idefault -g -r1000 -x -sstatic"
    "OCC + ATS (W1)"         "-idefault -g -r1000 -w1 -sadaptive"
    "OCC + ATS + DU (W1)"    "-idefault -g -r1000 -x -w1 -sadaptive"
    "OCC + ATS (W4)"         "-idefault -g -r1000 -w4 -sadaptive"
    "OCC + ATS + DU (W4)"    "-idefault -g -r1000 -x -w4 -sadaptive"
    "OCC + ATS (W0)"         "-idefault -g -r1000 -sadaptive"
    "OCC + ATS + DU (W0)"    "-idefault -g -r1000 -x -sadaptive"
    "TicToc (W1)"         "-itictoc -g -r1000 -w1 -snone"
    "TicToc + DU (W1)"    "-itictoc -g -r1000 -x -w1 -snone"
    "TicToc (W4)"         "-itictoc -g -r1000 -w4 -snone"
    "TicToc + DU (W4)"    "-itictoc -g -r1000 -x -w4 -snone"
    "TicToc (W0)"         "-itictoc -g -r1000 -snone"
    "TicToc + DU (W0)"    "-itictoc -g -r1000 -x -snone"
    "TicToc + STS (W1)"         "-itictoc -g -r1000 -w1 -sstatic"
    "TicToc + STS + DU (W1)"    "-itictoc -g -r1000 -x -w1 -sstatic"
    "TicToc + STS (W4)"         "-itictoc -g -r1000 -w4 -sstatic"
    "TicToc + STS + DU (W4)"    "-itictoc -g -r1000 -x -w4 -sstatic"
    "TicToc + STS (W0)"         "-itictoc -g -r1000 -sstatic"
    "TicToc + STS + DU (W0)"    "-itictoc -g -r1000 -x -sstatic"
    "TicToc + ATS (W1)"         "-itictoc -g -r1000 -w1 -sadaptive"
    "TicToc + ATS + DU (W1)"    "-itictoc -g -r1000 -x -w1 -sadaptive"
    "TicToc + ATS (W4)"         "-itictoc -g -r1000 -w4 -sadaptive"
    "TicToc + ATS + DU (W4)"    "-itictoc -g -r1000 -x -w4 -sadaptive"
    "TicToc + ATS (W0)"         "-itictoc -g -r1000 -sadaptive"
    "TicToc + ATS + DU (W0)"    "-itictoc -g -r1000 -x -sadaptive"
  )

  TPCC_MVCC=(
    "MVCC (W1)"         "-imvcc -g -r1000 -w1 -snone"
    "MVCC + DU (W1)"    "-imvcc -g -r1000 -x -w1 -snone"
    "MVCC (W4)"         "-imvcc -g -r1000 -w4 -snone"
    "MVCC + DU (W4)"    "-imvcc -g -r1000 -x -w4 -snone"
    "MVCC (W0)"         "-imvcc -g -r1000 -snone"
    "MVCC + DU (W0)"    "-imvcc -g -r1000 -x -snone"
    "MVCC + STS (W1)"         "-imvcc -g -r1000 -w1 -sstatic"
    "MVCC + STS + DU (W1)"    "-imvcc -g -r1000 -x -w1 -sstatic"
    "MVCC + STS (W4)"         "-imvcc -g -r1000 -w4 -sstatic"
    "MVCC + STS + DU (W4)"    "-imvcc -g -r1000 -x -w4 -sstatic"
    "MVCC + STS (W0)"         "-imvcc -g -r1000 -sstatic"
    "MVCC + STS + DU (W0)"    "-imvcc -g -r1000 -x -sstatic"
    "MVCC + ATS (W1)"         "-imvcc -g -r1000 -w1 -sadaptive"
    "MVCC + ATS + DU (W1)"    "-imvcc -g -r1000 -x -w1 -sadaptive"
    "MVCC + ATS (W4)"         "-imvcc -g -r1000 -w4 -sadaptive"
    "MVCC + ATS + DU (W4)"    "-imvcc -g -r1000 -x -w4 -sadaptive"
    "MVCC + ATS (W0)"         "-imvcc -g -r1000 -sadaptive"
    "MVCC + ATS + DU (W0)"    "-imvcc -g -r1000 -x -sadaptive"
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
    "MVCC (W1)"         "-imvcc -g -r1000 -w1 -snone"
    "MVCC + DU (W1)"    "-imvcc -g -r1000 -x -w1 -snone"
    "MVCC (W4)"         "-imvcc -g -r1000 -w4 -snone"
    "MVCC + DU (W4)"    "-imvcc -g -r1000 -x -w4 -snone"
    "MVCC (W0)"         "-imvcc -g -r1000 -snone"
    "MVCC + DU (W0)"    "-imvcc -g -r1000 -x -snone"
    "MVCC + STS (W1)"         "-imvcc -g -r1000 -w1 -sstatic"
    "MVCC + STS + DU (W1)"    "-imvcc -g -r1000 -x -w1 -sstatic"
    "MVCC + STS (W4)"         "-imvcc -g -r1000 -w4 -sstatic"
    "MVCC + STS + DU (W4)"    "-imvcc -g -r1000 -x -w4 -sstatic"
    "MVCC + STS (W0)"         "-imvcc -g -r1000 -sstatic"
    "MVCC + STS + DU (W0)"    "-imvcc -g -r1000 -x -sstatic"
    "MVCC + ATS (W1)"         "-imvcc -g -r1000 -w1 -sadaptive"
    "MVCC + ATS + DU (W1)"    "-imvcc -g -r1000 -x -w1 -sadaptive"
    "MVCC + ATS (W4)"         "-imvcc -g -r1000 -w4 -sadaptive"
    "MVCC + ATS + DU (W4)"    "-imvcc -g -r1000 -x -w4 -sadaptive"
    "MVCC + ATS (W0)"         "-imvcc -g -r1000 -sadaptive"
    "MVCC + ATS + DU (W0)"    "-imvcc -g -r1000 -x -sadaptive"
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
    "MVCC (W1)"         "-imvcc -g -r1000 -w1 -snone"
    "MVCC + DU (W1)"    "-imvcc -g -r1000 -x -w1 -snone"
    "MVCC (W4)"         "-imvcc -g -r1000 -w4 -snone"
    "MVCC + DU (W4)"    "-imvcc -g -r1000 -x -w4 -snone"
    "MVCC (W0)"         "-imvcc -g -r1000 -snone"
    "MVCC + DU (W0)"    "-imvcc -g -r1000 -x -snone"
    "MVCC + STS (W1)"         "-imvcc -g -r1000 -w1 -sstatic"
    "MVCC + STS + DU (W1)"    "-imvcc -g -r1000 -x -w1 -sstatic"
    "MVCC + STS (W4)"         "-imvcc -g -r1000 -w4 -sstatic"
    "MVCC + STS + DU (W4)"    "-imvcc -g -r1000 -x -w4 -sstatic"
    "MVCC + STS (W0)"         "-imvcc -g -r1000 -sstatic"
    "MVCC + STS + DU (W0)"    "-imvcc -g -r1000 -x -sstatic"
    "MVCC + ATS (W1)"         "-imvcc -g -r1000 -w1 -sadaptive"
    "MVCC + ATS + DU (W1)"    "-imvcc -g -r1000 -x -w1 -sadaptive"
    "MVCC + ATS (W4)"         "-imvcc -g -r1000 -w4 -sadaptive"
    "MVCC + ATS + DU (W4)"    "-imvcc -g -r1000 -x -w4 -sadaptive"
    "MVCC + ATS (W0)"         "-imvcc -g -r1000 -sadaptive"
    "MVCC + ATS + DU (W0)"    "-imvcc -g -r1000 -x -sadaptive"
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

setup_tpcc_mvcc_vp_1gc() {
  EXPERIMENT_NAME="TPC-C MVCC (1ms GC, vertical partitioning, REQUIRES MASTER BRANCH)"

  TPCC_OCC=(
  )

  TPCC_MVCC=(
    "MVCC (W1)"        "-imvcc -g -w1 -r1000"
    "MVCC + DU (W1)"   "-imvcc -g -x -w1 -r1000"
    "MVCC (W4)"        "-imvcc -g -w4 -r1000"
    "MVCC + DU (W4)"   "-imvcc -g -x -w4 -r1000"
    "MVCC (W0)"        "-imvcc -g -r1000"
    "MVCC + DU (W0)"   "-imvcc -g -x -r1000"
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
    "MVCC + DU (W1)"   "-imvcc -g -x -w1 -r1000"
    "MVCC (W4)"        "-imvcc -g -w4 -r1000"
    "MVCC + DU (W4)"   "-imvcc -g -x -w4 -r1000"
    "MVCC (W0)"        "-imvcc -g -r1000"
    "MVCC + DU (W0)"   "-imvcc -g -x -r1000"
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
    "MVCC + DU (W1)"   "-imvcc -g -x -w1 -r1000"
    "MVCC (W4)"        "-imvcc -g -w4 -r1000"
    "MVCC + DU (W4)"   "-imvcc -g -x -w4 -r1000"
    "MVCC (W0)"        "-imvcc -g -r1000"
    "MVCC + DU (W0)"   "-imvcc -g -x -r1000"
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
    "OCC (W1)"         "-idefault -g -r1000 -w1 -snone"
    "OCC + DU (W1)"    "-idefault -g -r1000 -x -w1 -snone"
    "OCC (W4)"         "-idefault -g -r1000 -w4 -snone"
    "OCC + DU (W4)"    "-idefault -g -r1000 -x -w4 -snone"
    "OCC (W0)"         "-idefault -g -r1000 -snone"
    "OCC + DU (W0)"    "-idefault -g -r1000 -x -snone"
    "OCC + STS (W1)"         "-idefault -g -r1000 -w1 -sstatic"
    "OCC + STS + DU (W1)"    "-idefault -g -r1000 -x -w1 -sstatic"
    "OCC + STS (W4)"         "-idefault -g -r1000 -w4 -sstatic"
    "OCC + STS + DU (W4)"    "-idefault -g -r1000 -x -w4 -sstatic"
    "OCC + STS (W0)"         "-idefault -g -r1000 -sstatic"
    "OCC + STS + DU (W0)"    "-idefault -g -r1000 -x -sstatic"
    "OCC + ATS (W1)"         "-idefault -g -r1000 -w1 -sadaptive"
    "OCC + ATS + DU (W1)"    "-idefault -g -r1000 -x -w1 -sadaptive"
    "OCC + ATS (W4)"         "-idefault -g -r1000 -w4 -sadaptive"
    "OCC + ATS + DU (W4)"    "-idefault -g -r1000 -x -w4 -sadaptive"
    "OCC + ATS (W0)"         "-idefault -g -r1000 -sadaptive"
    "OCC + ATS + DU (W0)"    "-idefault -g -r1000 -x -sadaptive"
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

setup_tpcc_opacity() {
  EXPERIMENT_NAME="TPC-C with Opacity"

  TPCC_OCC=(
    "OPQ (W1)"         "-iopaque -g -w1"
    "OPQ + DU (W1)"    "-iopaque -g -w1 -x"
    "OPQ (W4)"         "-iopaque -g -w4"
    "OPQ + DU (W4)"    "-iopaque -g -x -w4"
    "OPQ (W0)"         "-iopaque -g"
    "OPQ + DU (W0)"    "-iopaque -g -x"
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
    "MVCC + DU (W1)"    "-imvcc -g -w1 -x"
    "MVCC (W4)"         "-imvcc -g -w4"
    "MVCC + DU (W4)"    "-imvcc -g -x -w4"
    "MVCC (W0)"         "-imvcc -g"
    "MVCC + DU (W0)"    "-imvcc -g -x"
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
    "OCC + DU (W0)"    "-idefault -g -x"
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
    "OCC + DU (W1)"    "-idefault -g -w1 -x -r1000"
    "TicToc (W1)"      "-itictoc -n -g -w1 -r1000"
    "TicToc + DU (W1)" "-itictoc -n -g -w1 -x -r1000"
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
    "TicToc (W1)"         "-itictoc -g -r1000 -n -w1 -snone"
    "TicToc + DU (W1)"    "-itictoc -g -r1000 -n -x -w1 -snone"
    "TicToc (W4)"         "-itictoc -g -r1000 -n -w4 -snone"
    "TicToc + DU (W4)"    "-itictoc -g -r1000 -n -x -w4 -snone"
    "TicToc (W0)"         "-itictoc -g -r1000 -n -snone"
    "TicToc + DU (W0)"    "-itictoc -g -r1000 -n -x -snone"
    "TicToc + STS (W1)"         "-itictoc -g -r1000 -n -w1 -sstatic"
    "TicToc + STS + DU (W1)"    "-itictoc -g -r1000 -n -x -w1 -sstatic"
    "TicToc + STS (W4)"         "-itictoc -g -r1000 -n -w4 -sstatic"
    "TicToc + STS + DU (W4)"    "-itictoc -g -r1000 -n -x -w4 -sstatic"
    "TicToc + STS (W0)"         "-itictoc -g -r1000 -n -sstatic"
    "TicToc + STS + DU (W0)"    "-itictoc -g -r1000 -n -x -sstatic"
    "TicToc + ATS (W1)"         "-itictoc -g -r1000 -n -w1 -sadaptive"
    "TicToc + ATS + DU (W1)"    "-itictoc -g -r1000 -n -x -w1 -sadaptive"
    "TicToc + ATS (W4)"         "-itictoc -g -r1000 -n -w4 -sadaptive"
    "TicToc + ATS + DU (W4)"    "-itictoc -g -r1000 -n -x -w4 -sadaptive"
    "TicToc + ATS (W0)"         "-itictoc -g -r1000 -n -sadaptive"
    "TicToc + ATS + DU (W0)"    "-itictoc -g -r1000 -n -x -sadaptive"
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
    "TicToc + DU (W1)" "-itictoc -g -w1 -x -r1000"
    "TicToc (W4)"      "-itictoc -g -w4 -r1000"
    "TicToc + DU (W4)" "-itictoc -g -w4 -x -r1000"
    "TicToc (W0)"      "-itictoc -g -r1000"
    "TicToc + DU (W0)" "-itictoc -g -x -r1000"
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
    "OCC + DU"    "-idefault -b -x -snone"
    "OCC + STS"         "-idefault -b -sstatic"
    "OCC + STS + DU"    "-idefault -b -x -sstatic"
    "OCC + ATS"         "-idefault -b -sadaptive"
    "OCC + ATS + DU"    "-idefault -b -x -sadaptive"
    "TicToc"         "-itictoc -b -snone"
    "TicToc + DU"    "-itictoc -b -x -snone"
    "TicToc + STS"         "-itictoc -b -sstatic"
    "TicToc + STS + DU"    "-itictoc -b -x -sstatic"
    "TicToc + ATS"         "-itictoc -b -sadaptive"
    "TicToc + ATS + DU"    "-itictoc -b -x -sadaptive"
  )

  WIKI_MVCC=(
    "MVCC"         "-imvcc -b -snone"
    "MVCC + DU"    "-imvcc -b -x -snone"
    "MVCC + STS"         "-imvcc -b -sstatic"
    "MVCC + STS + DU"    "-imvcc -b -x -sstatic"
    "MVCC + ATS"         "-imvcc -b -sadaptive"
    "MVCC + ATS + DU"    "-imvcc -b -x -sadaptive"
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
    "OCC + DU"    "-idefault -b -x -snone"
    "OCC + STS"         "-idefault -b -sstatic"
    "OCC + STS + DU"    "-idefault -b -x -sstatic"
    "OCC + ATS"         "-idefault -b -sadaptive"
    "OCC + ATS + DU"    "-idefault -b -x -sadaptive"
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
    "MVCC + DU"    "-imvcc -b -x -snone"
    "MVCC + STS"         "-imvcc -b -sstatic"
    "MVCC + STS + DU"    "-imvcc -b -x -sstatic"
    "MVCC + ATS"         "-imvcc -b -sadaptive"
    "MVCC + ATS + DU"    "-imvcc -b -x -sadaptive"
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
    "TicToc + DU"    "-itictoc -b -x -snone"
    "TicToc + STS"         "-itictoc -b -sstatic"
    "TicToc + STS + DU"    "-itictoc -b -x -sstatic"
    "TicToc + ATS"         "-itictoc -b -sadaptive"
    "TicToc + ATS + DU"    "-itictoc -b -x -sadaptive"
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
    "OCC (A) + DU"    "-mA -idefault -g -x -snone"
    "OCC (A) + STS"         "-mA -idefault -g -sstatic"
    "OCC (A) + STS + DU"    "-mA -idefault -g -x -sstatic"
    "OCC (A) + ATS"         "-mA -idefault -g -sadaptive"
    "OCC (A) + ATS + DU"    "-mA -idefault -g -x -sadaptive"
    "TicToc (A)"         "-mA -itictoc -g -snone"
    "TicToc (A) + DU"    "-mA -itictoc -g -x -snone"
    "TicToc (A) + STS"         "-mA -itictoc -g -sstatic"
    "TicToc (A) + STS + DU"    "-mA -itictoc -g -x -sstatic"
    "TicToc (A) + ATS"         "-mA -itictoc -g -sadaptive"
    "TicToc (A) + ATS + DU"    "-mA -itictoc -g -x -sadaptive"
  )

  YCSB_MVCC=(
    "MVCC (A)"         "-mA -imvcc -g -snone"
    "MVCC (A) + DU"    "-mA -imvcc -g -x -snone"
    "MVCC (A) + STS"         "-mA -imvcc -g -sstatic"
    "MVCC (A) + STS + DU"    "-mA -imvcc -g -x -sstatic"
    "MVCC (A) + ATS"         "-mA -imvcc -g -sadaptive"
    "MVCC (A) + ATS + DU"    "-mA -imvcc -g -x -sadaptive"
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
    "OCC + DU (A)"    "-mA -idefault -g -x -snone"
    "OCC + STS (A)"         "-mA -idefault -g -sstatic"
    "OCC + STS + DU (A)"    "-mA -idefault -g -x -sstatic"
    "OCC + ATS (A)"         "-mA -idefault -g -sadaptive"
    "OCC + ATS + DU (A)"    "-mA -idefault -g -x -sadaptive"
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
    "OCC + DU (A)"    "-mA -idefault -g -x -snone"
    "OCC + STS (A)"         "-mA -idefault -g -sstatic"
    "OCC + STS + DU (A)"    "-mA -idefault -g -x -sstatic"
    "OCC + ATS (A)"         "-mA -idefault -g -sadaptive"
    "OCC + ATS + DU (A)"    "-mA -idefault -g -x -sadaptive"
    "TicToc (A)"         "-mA -itictoc -g -snone"
    "TicToc + DU (A)"    "-mA -itictoc -g -x -snone"
    "TicToc + STS (A)"         "-mA -itictoc -g -sstatic"
    "TicToc + STS + DU (A)"    "-mA -itictoc -g -x -sstatic"
    "TicToc + ATS (A)"         "-mA -itictoc -g -sadaptive"
    "TicToc + ATS + DU (A)"    "-mA -itictoc -g -x -sadaptive"
  )

  YCSB_MVCC=(
    "MVCC (A)"         "-mA -imvcc -g -snone"
    "MVCC + DU (A)"    "-mA -imvcc -g -x -snone"
    "MVCC + STS (A)"         "-mA -imvcc -g -sstatic"
    "MVCC + STS + DU (A)"    "-mA -imvcc -g -x -sstatic"
    "MVCC + ATS (A)"         "-mA -imvcc -g -sadaptive"
    "MVCC + ATS + DU (A)"    "-mA -imvcc -g -x -sadaptive"
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
    "TicToc + DU (A)"    "-mA -itictoc -g -x -snone"
    "TicToc + STS (A)"         "-mA -itictoc -g -sstatic"
    "TicToc + STS + DU (A)"    "-mA -itictoc -g -x -sstatic"
    "TicToc + ATS (A)"         "-mA -itictoc -g -sadaptive"
    "TicToc + ATS + DU (A)"    "-mA -itictoc -g -x -sadaptive"
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
    "MVCC + DU (A)"    "-mA -imvcc -g -x -snone"
    "MVCC + STS (A)"         "-mA -imvcc -g -sstatic"
    "MVCC + STS + DU (A)"    "-mA -imvcc -g -x -sstatic"
    "MVCC + ATS (A)"         "-mA -imvcc -g -sadaptive"
    "MVCC + ATS + DU (A)"    "-mA -imvcc -g -x -sadaptive"
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
    "OCC + DU (B)"    "-mB -idefault -g -x -snone"
    "OCC + STS (B)"         "-mB -idefault -g -sstatic"
    "OCC + STS + DU (B)"    "-mB -idefault -g -x -sstatic"
    "OCC + ATS (B)"         "-mB -idefault -g -sadaptive"
    "OCC + ATS + DU (B)"    "-mB -idefault -g -x -sadaptive"
    "TicToc (B)"         "-mB -itictoc -g -snone"
    "TicToc + DU (B)"    "-mB -itictoc -g -x -snone"
    "TicToc + STS (B)"         "-mB -itictoc -g -sstatic"
    "TicToc + STS + DU (B)"    "-mB -itictoc -g -x -sstatic"
    "TicToc + ATS (B)"         "-mB -itictoc -g -sadaptive"
    "TicToc + ATS + DU (B)"    "-mB -itictoc -g -x -sadaptive"
  )

  YCSB_MVCC=(
    "MVCC (B)"         "-mB -imvcc -g -snone"
    "MVCC + DU (B)"    "-mB -imvcc -g -x -snone"
    "MVCC + STS (B)"         "-mB -imvcc -g -sstatic"
    "MVCC + STS + DU (B)"    "-mB -imvcc -g -x -sstatic"
    "MVCC + ATS (B)"         "-mB -imvcc -g -sadaptive"
    "MVCC + ATS + DU (B)"    "-mB -imvcc -g -x -sadaptive"
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
    "OCC + DU (B)"    "-mB -idefault -g -x -snone"
    "OCC + STS (B)"         "-mB -idefault -g -sstatic"
    "OCC + STS + DU (B)"    "-mB -idefault -g -x -sstatic"
    "OCC + ATS (B)"         "-mB -idefault -g -sadaptive"
    "OCC + ATS + DU (B)"    "-mB -idefault -g -x -sadaptive"
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
    "OCC + DU (B)"    "-mB -idefault -g -x -snone"
    "OCC + STS (B)"         "-mB -idefault -g -sstatic"
    "OCC + STS + DU (B)"    "-mB -idefault -g -x -sstatic"
    "OCC + ATS (B)"         "-mB -idefault -g -sadaptive"
    "OCC + ATS + DU (B)"    "-mB -idefault -g -x -sadaptive"
    "TicToc (B)"         "-mB -itictoc -g -snone"
    "TicToc + DU (B)"    "-mB -itictoc -g -x -snone"
    "TicToc + STS (B)"         "-mB -itictoc -g -sstatic"
    "TicToc + STS + DU (B)"    "-mB -itictoc -g -x -sstatic"
    "TicToc + ATS (B)"         "-mB -itictoc -g -sadaptive"
    "TicToc + ATS + DU (B)"    "-mB -itictoc -g -x -sadaptive"
  )

  YCSB_MVCC=(
    "MVCC (B)"         "-mB -imvcc -g -snone"
    "MVCC + DU (B)"    "-mB -imvcc -g -x -snone"
    "MVCC + STS (B)"         "-mB -imvcc -g -sstatic"
    "MVCC + STS + DU (B)"    "-mB -imvcc -g -x -sstatic"
    "MVCC + ATS (B)"         "-mB -imvcc -g -sadaptive"
    "MVCC + ATS + DU (B)"    "-mB -imvcc -g -x -sadaptive"
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
    "TicToc + DU (B)"    "-mB -itictoc -g -x -snone"
    "TicToc + STS (B)"         "-mB -itictoc -g -sstatic"
    "TicToc + STS + DU (B)"    "-mB -itictoc -g -x -sstatic"
    "TicToc + ATS (B)"         "-mB -itictoc -g -sadaptive"
    "TicToc + ATS + DU (B)"    "-mB -itictoc -g -x -sadaptive"
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
    "MVCC + DU (B)"    "-mB -imvcc -g -x -snone"
    "MVCC + STS (B)"         "-mB -imvcc -g -sstatic"
    "MVCC + STS + DU (B)"    "-mB -imvcc -g -x -sstatic"
    "MVCC + ATS (B)"         "-mB -imvcc -g -sadaptive"
    "MVCC + ATS + DU (B)"    "-mB -imvcc -g -x -sadaptive"
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
    "OCC (C) + DU"    "-mC -idefault -g -x"
  )

  YCSB_MVCC=(
    "MVCC (C)"        "-mC -imvcc -g"
    "MVCC (C) + DU"   "-mC -imvcc -g -x"
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
    "OCC + DU (C)"    "-mC -idefault -g -x -snone"
    "OCC + STS (C)"         "-mC -idefault -g -sstatic"
    "OCC + STS + DU (C)"    "-mC -idefault -g -x -sstatic"
    "OCC + ATS (C)"         "-mC -idefault -g -sadaptive"
    "OCC + ATS + DU (C)"    "-mC -idefault -g -x -sadaptive"
    "TicToc (C)"         "-mC -itictoc -g -snone"
    "TicToc + DU (C)"    "-mC -itictoc -g -x -snone"
    "TicToc + STS (C)"         "-mC -itictoc -g -sstatic"
    "TicToc + STS + DU (C)"    "-mC -itictoc -g -x -sstatic"
    "TicToc + ATS (C)"         "-mC -itictoc -g -sadaptive"
    "TicToc + ATS + DU (C)"    "-mC -itictoc -g -x -sadaptive"
  )

  YCSB_MVCC=(
    "MVCC (C)"         "-mC -imvcc -g -snone"
    "MVCC + DU (C)"    "-mC -imvcc -g -x -snone"
    "MVCC + STS (C)"         "-mC -imvcc -g -sstatic"
    "MVCC + STS + DU (C)"    "-mC -imvcc -g -x -sstatic"
    "MVCC + ATS (C)"         "-mC -imvcc -g -sadaptive"
    "MVCC + ATS + DU (C)"    "-mC -imvcc -g -x -sadaptive"
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
    "OCC + DU (X)"    "-mX -idefault -g -x"
    "TicToc + DU (X)" "-mX -itictoc -g -x"
    "OCC (X)"         "-mX -idefault -g"
    "TicToc (X)"      "-mX -itictoc -g"
  )

  YCSB_MVCC=(
    "MVCC + DU (X)" "-mX -imvcc -g -x"
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
    "OCC + DU (Y)"    "-mY -idefault -g -x"
    "TicToc + DU (Y)" "-mY -itictoc -g -x"
    "OCC (Y)"         "-mY -idefault -g"
    "TicToc (Y)"      "-mY -itictoc -g"
  )

  YCSB_MVCC=(
    "MVCC + DU (Y)" "-mY -imvcc -g -x"
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
    "OCC + DU (Z)"    "-mZ -idefault -g -x"
    "TicToc + DU (Z)" "-mZ -itictoc -g -x"
    "OCC (Z)"         "-mZ -idefault -g"
    "TicToc (Z)"      "-mZ -itictoc -g"
  )

  YCSB_MVCC=(
    "MVCC + DU (Z)" "-mZ -imvcc -g -x"
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
