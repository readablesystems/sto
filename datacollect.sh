#!/bin/bash

OUT="$1"
N="$2"
OTHER_OPTIONS="$3"
CMD="./concurrent 4 --nthreads=4 --ntrans=1000000 --opspertrans=100 $OTHER_OPTIONS --writepercent="

make

float_scale=6
function float_eval()
{
    local stat=0
    local result=0.0
    if [[ $# -gt 0 ]]; then
        result=$(echo "scale=$float_scale; $*" | bc -q 2>/dev/null)
        stat=$?
        if [[ $stat -eq 0  &&  -z "$result" ]]; then stat=1; fi
    fi
    echo $result
    return $stat
}

percents=( 0 .1 .2 .3 .4 .5 .6 .7 .8 .9 1 )
echo "${percents[@]}"
for p in "${percents[@]}"; do
    echo "$CMD$p"
    # don't count first run which is usually off by a bit
    $CMD$p > /dev/null 2> /dev/null
   # AVG=0.0
    for i in `seq 1 $N`; do
        RES=$($CMD$p)
        echo $RES
        echo $RES >> $OUT
#        AVG=$(float_eval "$AVG + $RES")
    done
 #   AVG=$(float_eval "$AVG / $N")
  #  echo $AVG >> $OUT
done
