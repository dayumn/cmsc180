#!/bin/bash

# lab activity 2: impyerno
SOURCE_FILE="lab02_row.c"
OUTPUT_EXE="lab02_row"
CSV_FILE="results_act2.csv"
N_VALUES=(30000 40000 50000 100000)
T_VALUES=(1 2 4 8 16 32 64)

# compile 
echo "Compiling $SOURCE_FILE..."
gcc "$SOURCE_FILE" -o "$OUTPUT_EXE" -lpthread

if [ $? -ne 0 ]; then
    echo "Compilation failed! Exiting."
    exit 1
fi

# prepare the csv file headers
echo "n,t,Run 1,Run 2,Run 3" > "$CSV_FILE"

# loop through each n and t
for n in "${N_VALUES[@]}"; do
    for t in "${T_VALUES[@]}"; do
        row_data="$n,$t"
        echo "Testing n=$n, t=$t"

        for i in {1..3}; do
            result=$(echo "$n $t" | ./"$OUTPUT_EXE" 2>&1 | tail -n 1)

            # check if process was killed (out of memory)
            if [ $? -ne 0 ]; then
                echo "  Run $i FAILED (likely out of memory)"
                result="FAIL"
            fi

            row_data="$row_data,$result"
        done

        echo "$row_data" >> "$CSV_FILE"
    done
done

echo "Done! Results saved to $CSV_FILE"
