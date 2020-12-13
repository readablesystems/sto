#!/bin/bash

MAX_RETRIES=10
ITERS=5
THREADS=(1 2 4 12 24 32 40 48 64)
TIMEOUT=20  # In seconds
HUGEPAGES=102400  # 49152 for stoo, 102400 for AWS
DRY_RUN=0  # >0 means do a dry run

. run_config.sh

setup_tpcc_occ  # Change this accordingly!

printf "Experiment: $EXPERIMENT_NAME ($ITERS trials)\n"

ALL_BINARIES=("${OCC_BINARIES[@]}" "${MVCC_BINARIES[@]}")

run_bench () {
  OUTFILE=$1
  DELIVERY_OUTFILE=$2
  shift 2
  BINARY=$1
  shift
  CT_FLAGS=$1  # Compile-time flags
  shift
  HEADER_LABEL=$1
  shift
  ITERS=$1
  shift
  THREADS=$1
  shift
  FLAGS=()
  LABELS=()
  FL_COUNT=$((${#@} / 2))
  for i in $(seq $FL_COUNT)
  do
    LABELS+=("$1")
    shift
    FLAGS+=("$1")
    shift
  done

  # Turn off reserved huge pages if libc malloc is used.
  if [[ $CT_FLAGS == *"USE_LIBCMALLOC=1"* ]]
  then
    ./mount_hugepages.sh 0
  fi

  if [ ${#FLAGS[@]} -ne ${#LABELS[@]} ]
  then
    printf "Need equal number of flag parameters (${#FLAGS[@]}) and labels (${#LABELS[@]})\n"
    exit 1
  fi
  printf "# Threads" >> $OUTFILE
  printf "# Threads" >> $DELIVERY_OUTFILE
  for label in "${LABELS[@]}"
  do
    for k in $(seq 1 $ITERS)
    do
      printf ",$label$HEADER_LABEL [T$k]" >> $OUTFILE
      printf ",$label$HEADER_LABEL [T$k]" >> $DELIVERY_OUTFILE
    done
  done
  printf "\n" >> $OUTFILE
  printf "\n" >> $DELIVERY_OUTFILE
  for i in ${THREADS[*]}
  do
    printf "$i" >> $OUTFILE
    printf "$i" >> $DELIVERY_OUTFILE
    for f in "${FLAGS[@]}"
    do
      k=0
      while [ $k -lt $ITERS ]
      do
        runs=1
        while [ $runs -le $MAX_RETRIES ]
        do
          cmd="./$BINARY -t$i $f"
          update_cmd
          printf "\rTrial $(($k + 1)), run $runs times: $cmd"
          if [ $DRY_RUN -gt 0 ]
          then
              printf "\n"
              break
          fi
          $cmd 2>$TEMPERR >$TEMPOUT &
          pid=$!
          sleep $TIMEOUT && kill -0 $pid 2&>/dev/null && kill -9 $pid &
          wait $pid
          result=$(cat $TEMPOUT | grep -e '^Throughput:' | grep -oE '[0-9.]+')
          real_time_ms=$(cat $TEMPOUT | grep -e '^Real time:' | grep -oE '[0-9.]+')
          delivery=$(cat $TEMPERR | grep -e '^\$      Delivery:' | grep -oE '[0-9]+\(' | sed 's/(//')
          if [ "$delivery" != "" ]
          then
            delivery=$(echo "$delivery $real_time_ms" | awk '{print $1/($2/1000)}')
          fi
          sleep 2
          if [ $(grep 'next commit-tid' $TEMPERR | wc -l) -ne 0 ]
          then
            break
          fi
          runs=$(($runs + 1))
        done
        if [ $runs -gt 1 ]
        then
          printf "\n"
        else
          printf "\r"
          for n in $(seq 1 7)
          do
            printf "           "
          done
          printf "\r"
        fi
        if [ $runs -lt $MAX_RETRIES ]
        then
          printf ",$result" >> $OUTFILE
          printf ",$delivery" >> $DELIVERY_OUTFILE
          k=$(($k + 1))
        else
          while [ $k -lt $ITERS ]
          do
            printf ",DNF" >> $OUTFILE
            printf ",DNF" >> $DELIVERY_OUTFILE
            k=$(($k + 1))
          done
        fi
      done
    done
    printf "\n" >> $OUTFILE
    printf "\n" >> $DELIVERY_OUTFILE
  done

  # Turn disabled huge pages back on.
  if [[ $CT_FLAGS == *"USE_LIBCMALLOC=1"* ]]
  then
    ./mount_hugepages.sh $HUGEPAGES
  fi
}

compile() {
  while [ $# -gt 0 ]
  do
    TARGET=$1
    SUFFIX=$2
    BINARY="$TARGET$SUFFIX"
    FLAGS=$3
    shift 4
    if [ -e $BINARY ]
    then
      echo "Reusing existing $BINARY"
    else
      echo "Compiling $BINARY with $FLAGS"
      make clean > /dev/null
      make -j $TARGET $FLAGS > /dev/null
      mv $TARGET $BINARY
    fi
  done
}

run() {
  IS_MVCC=$1
  shift
  printf "stdout and stderr written to: %s\r\n" $TEMPDIR
  while [ $# -gt 0 ]
  do
    TARGET=$1
    SUFFIX=$2
    BINARY="$TARGET$SUFFIX"
    CT_FLAGS=$3
    HEADER_LABEL=$4
    shift 4

    OUTFILE=$RFILE
    DELIVERY_OUTFILE=$DFILE
    if [ -f $RFILE ]
    then
      OUTFILE=results/rtemp.txt
    fi
    if [ -f $DFILE ]
    then
      DELIVERY_OUTFILE=results/dtemp.txt
    fi
    if [ $IS_MVCC -gt 0 ]
    then
      printf "Running MVCC on $BINARY $HEADER_LABEL\n"
      run_bench $OUTFILE $DELIVERY_OUTFILE $BINARY "$CT_FLAGS" "$HEADER_LABEL" $ITERS $THREADS "${MVCC_LABELS[@]}"
    else
      printf "Running OCC on $BINARY $HEADER_LABEL\n"
      run_bench $OUTFILE $DELIVERY_OUTFILE $BINARY "$CT_FLAGS" "$HEADER_LABEL" $ITERS $THREADS "${OCC_LABELS[@]}"
    fi
    if [ $RFILE != $OUTFILE ]
    then
      mv $RFILE results/rcopy.txt
      join --header -t , -j 1 results/rcopy.txt $OUTFILE > $RFILE
      rm results/rcopy.txt $OUTFILE
    fi
    if [ $DFILE != $DELIVERY_OUTFILE ]
    then
      mv $DFILE results/dcopy.txt
      join --header -t , -j 1 results/dcopy.txt $DELIVERY_OUTFILE > $DFILE
      rm results/dcopy.txt $DELIVERY_OUTFILE
    fi
  done
}

default_call_runs() {
  ./mount_hugepages.sh $HUGEPAGES

  # Run OCC
  run 1 "${MVCC_BINARIES[@]}"

  # Run MVCC
  run 0 "${OCC_BINARIES[@]}"

  ./mount_hugepages.sh 0
}

estimate_runtime() {
  sets=$1
  seconds=$(($sets * (${#THREADS[@]} * $ITERS * 15 + 60) * 3))
  minutes=$(($seconds / 60 % 60))
  hours=$(($seconds / 3600 % 24))
  days=$(($seconds / 86400))
  seconds=$(($seconds % 60))
  printf "Estimated runtime: %d:%02d:%02d:%02d\r\n" $days $hours $minutes $seconds
}

if [ ${#ALL_BINARIES[@]} -eq 0 ]
then
  echo "No binaries selected! Did you remember to set up an experiment?"
  exit 1
fi

estimate_runtime $(((${#ALL_BINARIES[@]}) / 3))

start_time=$(date +%s)

compile "${ALL_BINARIES[@]}"

rm -rf results
mkdir results
RFILE=results/tpcc_occ_results.txt
DFILE=results/tpcc_occ_delivery_results.txt
TEMPDIR=$(mktemp -d /tmp/sto-XXXXXX)
TEMPERR="$TEMPDIR/err"
TEMPOUT="$TEMPDIR/out"

call_runs

end_time=$(date +%s)
runtime=$(($end_time - $start_time))

python3 /home/ubuntu/send_email.py --exp="$EXPERIMENT_NAME" --runtime=$runtime $RFILE $DFILE
if [ $DRY_RUN -eq 0 ] && [ "$METARUN" == "" ]
then
  # delay shutdown for 1 minute just in case
  sudo shutdown -h +1
fi
