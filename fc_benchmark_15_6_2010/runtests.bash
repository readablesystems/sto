#!/bin/bash

/usr/sbin/psrset -d 0
/usr/sbin/psrset -d 1
/usr/sbin/psrset -d 2
/usr/sbin/psrset -d 3
/usr/sbin/psrset -d 4
/usr/sbin/psrset -c 0-63

rm -f results
rm -f tblResults

isviewary="0 64 128 192 256 320 384"
isviewary="0"

percents="50_50"
threads="1 2 4 8 12 16 24 32 40 48 56 64"
#threads="1 2 4 8 12 16 24 32"
#threads="1 8 16 32 48 64" 
#threads="1 2 8 48"
#threads="1 2 4 8 12 16 24 32"
#threads="1 2 4 8 12 16 32 48 56 64 72 80"
#threads="1 2 4 8 12 32 48 64 80"
#threads="12 16 32"
#threads="16 32 64 80"
#threads="1 16 32 64"
#threads="1 4 8 16 32 63"
#threads="56"
#algorithms="fcstack lfstack elstack"
#algorithms="fclist fclistc fcpq lazylist"
#algorithms="fcqueue msqueue basketqueue ctqueue oyqueue oyqueuecom lsqueue"
#algorithms="fcstack lfstack elstack"
#algorithms="fcstack lfstack elstack"
#algorithms="fcskiplist fcpairheap fcskiplist lfskiplist lazyskiplist"
algorithms="ctqueue fcqueue msqueue basketqueue ctqueue oyqueue oyqueuecom lsqueue"
#algorithms="tmlist"
#algorithms="fcsoftheap fcpairheap"

#tests="TIME Idle_strands Instr_cnt DC_miss L2_dmiss_ld DTLB_miss CPU_st_to_PCX MMU_ld_to_PCX"
#tests="L2_dmiss_ld"
tests="TIME"
#tests="DC_miss"

capacities="20000"

rep="1"

count=0

for test in $tests; do

rm -f $test

for algorithm in $algorithms; do

for isview in $isviewary; do

for percent in $percents; do

for capacity in $capacities; do

for thread in $threads; do

	count=$(($count + 1))

        line="$algorithm 1 non 0 non 0 non 0 $count $thread $percent 0.0 $capacity 10 0 $isview 0"
        line=`echo $line | sed 's/_/ /g'`
        echo "$line" 1>&2
        echo -n "$line" >> $test

        for rep1 in $rep; do

           if [ $test == "TIME" ]
             then
              ./test_intel64 $line  >> $test
           fi

           if [ $test != "TIME" ]
             then
              cputrack -T 9999 -e -f -c $test ./test_sparc32 $line | awk '($4 == "lwp_exit") {printf " %d\n",$5}' | awk '{tot=tot+$1} END {printf "%d", tot}' >> $test
#               cputrack -T 9999 -e -f -c $test ./test_sparc64 $line | awk '($4 == "lwp_exit") {printf " %d\n",$5}' >> $test
           fi

           cp $test results
           echo -n " " >> results
           echo $test >> results
        done;
        echo >> $test

done; done; done; done; done; done;

cat results | grep lwp_exit | awk '($3 > 18) && ($5 > 0) {print $3, $5, $6}' > cache_miss.txt
