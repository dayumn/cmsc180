#!/bin/bash

# --- CONFIGURATION ---
SOURCE_FILE="lab01.c"       # Your C file name
OUTPUT_EXE="lab01"         # The executable name
CSV_FILE="results.csv"     # The output CSV file name
INPUTS=(100 200 300 400 500 600 700 800 900 1000 2000 4000 8000 16000 20000) # Add the sizes you want to test here
# ---------------------

# 1. Compile the program
echo "Compiling $SOURCE_FILE..."
gcc "$SOURCE_FILE" -o "$OUTPUT_EXE"

# Check if compilation succeeded
if [ $? -ne 0 ]; then
    echo "Compilation failed! Exiting."
    exit 1
fi

# 2. Prepare the CSV file (Header)
# This creates columns: Input Size, Run 1, Run 2, Run 3
echo "Input Size,Run 1,Run 2,Run 3" > "$CSV_FILE"

# 3. Loop through each input size (3, 500, etc.)
for n in "${INPUTS[@]}"; do
    #Recompile
   
    gcc "$SOURCE_FILE" -o "$OUTPUT_EXE"
    echo "Recompiled"
    # Start the row with the input size
    row_data="$n"
    echo "Testing input size: $n"

    # Run the program 3 times for this specific size
    for i in {1..3}; do
        # Run the program with input redirection (<<<)
        # We capture the output into a variable.
        # 'tail -n 1' ensures we grab the LAST line printed (usually the result/time)
        result=$(./"$OUTPUT_EXE" <<< "$n" | tail -n 1)
       
        # Append the result to our current CSV row
        row_data="$row_data,$result"
    done

    # Save the completed row to the CSV file
    echo "$row_data" >> "$CSV_FILE"
done

echo "Done! Results saved to $CSV_FILE"