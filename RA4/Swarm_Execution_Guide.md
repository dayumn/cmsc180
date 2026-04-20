# ICS Compute Swarm - Step-by-Step Execution Guide (LRP04)

This guide provides the exact terminal commands and steps to run your matrix distribution program on the ICS Compute Swarm, using the `echo` method to automatically feed inputs into your program.

**Assumption for this initial walkthrough:** We are testing matrix size **$n=4000$** with **$t=2$** slaves.

---

## Phase 1: Local Preparation (Terminal 1)
You will keep one terminal open on your local machine to compile your code, generate configuration files, and send them to the Overqueen.

1. **Navigate to your workspace:**
   ```bash
   cd /home/acer/Desktop/bry/cmsc180/RA4
   ```

2. **Compile your C program:**
   ```bash
   gcc lab04.c -o lab04 -Wall
   ```

3. **Set the Master Drone's IP** (Assuming Drone 1: `10.0.9.124` is Master):
   ```bash
   echo "10.0.9.124" > config_master.txt
   ```

4. **Generate the Slave Configuration** for 2 slaves:
   ```bash
   ./generate_slaves_config.sh 2
   ```
   *(This script automatically writes the IPs for Drone 2 and Drone 3, along with their ports, into `config_slaves.txt`)*

5. **Transfer the files to the Overqueen:**
   Upload the compiled executable and both text files to the swarm.
   ```bash
   scp lab04 config_master.txt config_slaves.txt jlpena1@overqueen:/home/jlpena1
   ```
   *(Use your MD5 hash password when prompted).*

---

## Phase 2: Connecting to the Swarm Drones
Leave Terminal 1 as it is. Open **three new terminal windows or tabs** (Terminals 2, 3, and 4).

1. In **Terminal 2**, SSH into **Slave 1 (Drone 2)**:
   ```bash
   ssh jlpena1@10.0.9.156
   ```

2. In **Terminal 3**, SSH into **Slave 2 (Drone 3)**:
   ```bash
   ssh jlpena1@10.0.9.131
   ```

3. In **Terminal 4**, SSH into the **Master (Drone 1)**:
   ```bash
   ssh jlpena1@10.0.9.124
   ```

---

## Phase 3: Executing the Experiment ($n=4000, t=2$)
**CRITICAL:** Always start the slave programs first so they are actively listening for the master. We will use `echo` to automatically pass the `n`, `p`, and `s` arguments into the `scanf()` prompt.

1. In **Terminal 2** (Slave 1), listen on Port 8001:
   ```bash
   echo "4000 8001 1" | ./lab04
   ```

2. In **Terminal 3** (Slave 2), listen on Port 8002:
   ```bash
   echo "4000 8002 1" | ./lab04
   ```

3. In **Terminal 4** (Master), initiate the distribution (Master uses an arbitrary terminal port, e.g., 8080):
   ```bash
   echo "4000 8080 0" | ./lab04
   ```

4. **Record your data**: The Master terminal (Terminal 4) will eventually print the `time_elapsed`. Copy this value into your `results.csv` table for $n=4000$ and $t=2$.

---

## Phase 4: Scaling up to $t=4, 8, 16$
To run the next set of experiments, you do not need to recompile the C code. You only need to change the configuration and open more SSH terminals.

1. Go back to your **Local Terminal (Terminal 1)**.
2. Generate the new config for 4 slaves:
   ```bash
   ./generate_slaves_config.sh 4
   ```
3. Upload *only* the new config to the Overqueen (the executable is already there):
   ```bash
   scp config_slaves.txt jlpena1@overqueen:/home/jlpena1
   ```
4. Open **2 additional terminal windows** (Terminals 5 & 6) and SSH into the new drones (Drone 4 & Drone 5 based on the IP list).
5. Run the `echo ... | ./lab04` command on **all 4 Slave Terminals**, incrementing their ports (8001, 8002, 8003, 8004).
6. Run the master command on the **Master Terminal**.

Repeat this scaling process for $t=8$ and $t=16$, generating the config, sending it to the Overqueen, and opening more SSH connections for the new drones.
