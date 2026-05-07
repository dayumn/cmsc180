#!/bin/bash

# Remove previous results
rm -f results.csv results_slaves.csv

echo "Compiling..."
gcc -O3 lab05_coreaffine.c -o lab05_coreaffine -Wall

MASTER_CSV="results.csv"
SLAVE_CSV="results_slaves.csv"
BIN="./lab05_coreaffine"

echo "n,t,Run 1,Run 2,Run 3,Average" > "$MASTER_CSV"
echo "n,t,Max of Run 1,Max of Run 2,Max of Run 3,Average" > "$SLAVE_CSV"

# The master is on local machine as well
LOCAL_IP="10.0.4.143"
echo "$LOCAL_IP" > config_master.txt

BASE_PORT=8000

total_start=$(date +%s)

for n in 4000 8000 16000; do
    for t in 2 4 8 16; do
        echo "Testing Matrix Size n=$n with Slaves t=$t..."

        # Create config_slaves.txt dynamically
        echo "$t" > config_slaves.txt
        for ((i=1; i<=t; i++)); do
            echo "$LOCAL_IP $((BASE_PORT+i))" >> config_slaves.txt
        done

        master_runs=()
        slave_max_runs=()
        master_sum=0
        slave_sum=0

        for run in 1 2 3; do
            echo "  -> Run $run..."
            killall lab05_coreaffine 2>/dev/null
            sleep 0.5

            mkdir -p slave_logs

            # Start t slaves in the background
            for ((i=1; i<=t; i++)); do
                $BIN <<< "$n $((BASE_PORT+i)) 1" > "slave_logs/run${run}_rank${i}.log" 2>&1 &
            done

            # Allow slaves to reach accept()
            sleep 0.5

            # Run master and extract the last printed line (execution time float)
            master_out=$($BIN <<< "$n $BASE_PORT 0" 2>/dev/null | tail -n 1)

            if ! [[ $master_out =~ ^[0-9]+\.[0-9]+$ ]]; then
                master_out=0.000000
            fi

            master_runs+=("$master_out")
            master_sum=$(awk "BEGIN {print $master_sum + $master_out}")

            # Wait for any background slaves to cleanly exit
            wait 2>/dev/null

            # Parse slave logs and take the max time for this run
            max_slave=0.000000
            for ((i=1; i<=t; i++)); do
                log_file="slave_logs/run${run}_rank${i}.log"
                slave_time=$(awk '/^[0-9]+\.[0-9]+$/{val=$0} END{if (val=="") val=0.000000; print val}' "$log_file")
                max_slave=$(awk "BEGIN {print ($slave_time > $max_slave) ? $slave_time : $max_slave}")
            done

            slave_max_runs+=("$max_slave")
            slave_sum=$(awk "BEGIN {print $slave_sum + $max_slave}")
        done

        master_avg=$(awk "BEGIN {printf \"%.6f\", $master_sum / 3}")
        slave_avg=$(awk "BEGIN {printf \"%.6f\", $slave_sum / 3}")

        echo "$n,$t,${master_runs[0]},${master_runs[1]},${master_runs[2]},$master_avg" >> "$MASTER_CSV"
        echo "$n,$t,${slave_max_runs[0]},${slave_max_runs[1]},${slave_max_runs[2]},$slave_avg" >> "$SLAVE_CSV"

        echo "  -> Completed: Master Avg = $master_avg seconds"
        echo "  -> Completed: Slave Max Avg = $slave_avg seconds"

        # Final cleanup for the next iteration
        killall lab05_coreaffine 2>/dev/null
    done
done

total_end=$(date +%s)
echo "All test configurations finished in $((total_end - total_start)) seconds!"
echo "Check $MASTER_CSV and $SLAVE_CSV for your tables."
