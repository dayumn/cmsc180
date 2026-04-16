#!/usr/bin/env bash
# remove set -e to prevent aborts
ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT_DIR"

# Compile if needed
if [ ! -x ./lab04 ]; then
  if [ -f lab04.c ]; then
    echo "Compiling lab04.c..."
    gcc -O2 lab04.c -o lab04 -lm
  else
    echo "lab04 executable or source not found in $ROOT_DIR" >&2
    exit 1
  fi
fi

OUTFILE="results1.csv"
echo "n,t,run1,run2,run3,avg" > "$OUTFILE"

# test parameters
N_VALUES=(4000 8000 16000)
T_VALUES=(2 4 8 16)

# Remote execution toggle: set to true if you have passwordless SSH to the drones
USE_SSH=true
SSH_USER="jlpena1" # Change this if your swarm username is different

if [ "$USE_SSH" = true ]; then
  echo "Distributing lab04 executable to all drone nodes..."
  readarray -t ALL_DRONES < <(awk '{print $1}' drone_pool.txt)
  for ip in "${ALL_DRONES[@]}"; do
    # Copy lab04 silently to their home folder
    sshpass -p '23b6f324a55f174da485747b51db7c6a' scp -o StrictHostKeyChecking=no lab04 "$SSH_USER@$ip:~/" >/dev/null 2>&1 &
  done
  wait
  echo "Distribution complete!"
fi

for n in "${N_VALUES[@]}"; do
  for t in "${T_VALUES[@]}"; do
    echo "Running test n=$n t=$t"

    runs=()

    for run in 1 2 3; do
      # generate config_master.txt dynamically for this t using the drone pool
      echo "$t" > config_master.txt
      head -n "$t" drone_pool.txt >> config_master.txt

      # Extract IP and port arrays
      readarray -t SLAVE_IPS < <(head -n "$t" drone_pool.txt | awk '{print $1}')
      readarray -t SLAVE_PORTS < <(head -n "$t" drone_pool.txt | awk '{print $2}')

      # start slaves
      pids=()
      for ((i=0;i<t;i++)); do
        ip="${SLAVE_IPS[i]}"
        port="${SLAVE_PORTS[i]}"
        
        if [ "$USE_SSH" = true ]; then
            # Remote start via SSH with sshpass for password injection (run in their home "~" instead of "$PWD" since paths differ)
            sshpass -p '23b6f324a55f174da485747b51db7c6a' ssh -o StrictHostKeyChecking=no -o LogLevel=ERROR "$SSH_USER@$ip" "cd ~ && SLAVE_T=$t printf '%d %d %d\n' $n $port 1 | ./lab04" >/dev/null 2>&1 &
            pids+=("$!")
        else
            # For pure local testing, you would run them locally. If you run them manually, comment this out.
            # Local start for testing (since IPs are remote, this will fail if master talks to remote while slaves are local)
            printf "%d %d %d\n" "$n" "$port" 1 | SLAVE_T=$t ./lab04 >/dev/null 2>&1 &
            pids+=("$!")
        fi
      done

      # small delay to ensure slaves are listening (ssh connections take time)
      sleep 3

      # run master and capture last numeric output line as elapsed time
      time_out=$(printf "%d %d %d\n" "$n" 5000 0 | ./lab04 2>/dev/null | tail -n1 || true)

      # wait for slaves to finish
      for pid in "${pids[@]}"; do
        wait "$pid" || true
      done

      # sanitize numeric output
      if [[ "$time_out" =~ ^[0-9]+\.[0-9]+$ ]]; then
        runs+=("$time_out")
      else
        # fallback: attempt to extract a float
        float=$(echo "$time_out" | grep -oE '[0-9]+\.[0-9]+' || true)
        runs+=("${float:-0}")
      fi

      echo "  run $run: ${runs[-1]}s"
      # short pause between runs
      sleep 0.1
    done

    # compute average
    sum=0
    for v in "${runs[@]}"; do
      sum=$(awk -v a="$sum" -v b="$v" 'BEGIN{print a+b}')
    done
    avg=$(awk -v s="$sum" -v c="${#runs[@]}" 'BEGIN{if(c>0)print s/c; else print 0}')

    # append to CSV
    printf "%s,%s,%s,%s,%s,%s\n" "$n" "$t" "${runs[0]}" "${runs[1]}" "${runs[2]}" "$avg" >> "$OUTFILE"
  done
done

echo "All tests complete. Results saved to $OUTFILE"
