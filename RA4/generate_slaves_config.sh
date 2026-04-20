#!/bin/bash

# Check if the number of slaves (t) was provided
if [ -z "$1" ]; then
    echo "Usage: ./generate_slaves_config.sh <number_of_slaves>"
    echo "Example: ./generate_slaves_config.sh 4"
    exit 1
fi

T=$1

# List of 16 slave drone IPs (Drones 2 to 17)
DRONES=(
    "10.0.9.156" # Drone 2
    "10.0.9.131" # Drone 3
    "10.0.9.109" # Drone 4
    "10.0.9.125" # Drone 5
    "10.0.9.162" # Drone 6
    "10.0.9.137" # Drone 7
    "10.0.9.117" # Drone 8
    "10.0.9.111" # Drone 9
    "10.0.9.135" # Drone 10
    "10.0.9.180" # Drone 11
    "10.0.9.174" # Drone 12
    "10.0.9.129" # Drone 13
    "10.0.9.184" # Drone 14
    "10.0.9.171" # Drone 15
    "10.0.9.163" # Drone 16
    "10.0.9.134" # Drone 17
)

# Ensure T does not exceed the available drones
if [ "$T" -gt "${#DRONES[@]}" ]; then
    echo "Error: Requested $T slaves, but only ${#DRONES[@]} IPs are configured."
    exit 1
fi

# 1. Write the number of slaves 't' to the first line
echo "$T" > config_slaves.txt

# 2. Loop to add the correct number of IPs and incrementing ports
for (( i=0; i<$T; i++ )); do
    PORT=$((8001 + i))
    echo "${DRONES[$i]} $PORT" >> config_slaves.txt
done

echo "Successfully generated config_slaves.txt for $T slaves!"
echo "--- config_slaves.txt ---"
cat config_slaves.txt
echo "-------------------------"

