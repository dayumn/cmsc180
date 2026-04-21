#!/bin/bash

# Remove previous results
rm -f results_affine.csv

echo "Compiling..."
gcc -O3 lab04_coreaffine.c -o lab04 -Wall

CSV_FILE="results_affine.csv"
echo "n,t,Run 1,Run 2,Run 3,Average" > $CSV_FILE

# The master is on local machine as well
echo "10.0.4.35" > config_master.txt

total_start=$(date +%s)

for n in 4000 8000 16000; do
    for t in 2 4 8 16; do
        echo "Testing Matrix Size n=$n with Slaves t=$t..."
        
        # Create config_slaves.txt dynamically
        echo "$t" > config_slaves.txt
        for ((i=1; i<=t; i++)); do
            echo "10.0.4.35 $((8000+i))" >> config_slaves.txt
        done
        
        runs=()
        sum=0
        for run in 1 2 3; do
            echo "  -> Run $run..."
            killall lab04 2>/dev/null
            sleep 0.5
            
            # Start t slaves in the background
            for ((i=1; i<=t; i++)); do
                ./lab04 <<< "$n $((8000+i)) 1" > /dev/null 2>&1 &
            done
            
            # Crucial: sleep longer to ensure all background slaves spawned and reached accept()
            sleep 0.5
            
            # Run master and extract the last printed line (which should be the execution time float)
            output=$(./lab04 <<< "$n 8000 0" 2>/dev/null | tail -n 1)
            
            # Safety check: if output isn't a float, default to 0 to prevent awk crashing
            if ! [[ $output =~ ^[0-9]+\.[0-9]+$ ]]; then
                output=0.000000
            fi
            
            runs+=("$output")
            sum=$(awk "BEGIN {print $sum + $output}")
            
            # Wait for any background slaves to cleanly exit
            wait 2>/dev/null
        done
        
        avg=$(awk "BEGIN {printf \"%.6f\", $sum / 3}")
        echo "$n,$t,${runs[0]},${runs[1]},${runs[2]},$avg" >> $CSV_FILE
        echo "  -> Completed: Avg Runtime = $avg seconds"
        
        # Final cleanup for the next iteration
        killall lab04 2>/dev/null
    done
done

total_end=$(date +%s)
echo "All test configurations finished in $((total_end - total_start)) seconds!"
echo "Check $CSV_FILE for your table report."
