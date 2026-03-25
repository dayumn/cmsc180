#!/bin/bash

# --- CONFIGURATION ---
SOURCE_FILE="lab02.c"
OUTPUT_EXE="lab02"
CSV_FILE="results.csv"
N_VALUES=(25000)
T_VALUES=(1 2 4 8 16 32 64)
# ---------------------

# 1. Compile the program
echo "Compiling $SOURCE_FILE..."
gcc -O2 "$SOURCE_FILE" -o "$OUTPUT_EXE" -lpthread

if [ $? -ne 0 ]; then
    echo "Compilation failed! Exiting."
    exit 1
fi

# 2. Prepare the CSV file (Header)
echo "n,t,Run 1,Run 2,Run 3" > "$CSV_FILE"

# 3. Loop through each n and t
for n in "${N_VALUES[@]}"; do
    for t in "${T_VALUES[@]}"; do
        row_data="$n,$t"
        echo "Testing n=$n, t=$t"

        for i in {1..3}; do
            result=$(echo "$n $t" | ./"$OUTPUT_EXE" | tail -n 1)
            row_data="$row_data,$result"
        done

        echo "$row_data" >> "$CSV_FILE"
    done
done

echo "Done! Results saved to $CSV_FILE"
