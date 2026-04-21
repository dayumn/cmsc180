# AI Coding Agent Instructions

This repository contains C source code and bash scripts for CMSC180 labs (RA1-RA4), focusing on evolving parallelization strategies (Sequential -> Pthreads -> Distributed memory via Sockets).

## Build and Run Conventions
* **Compilation**: `gcc` is primarily used without Makefiles. You must manually invoke `gcc` with the appropriate flags for the target lab.
  * RA1 (Sequential): `gcc lab01.c -o lab01`
  * RA2/RA3 (Pthreads): Ensure you compile with `-lpthread` and optionally `-O2`, e.g., `gcc -O2 lab02.c -o lab02 -lpthread`
  * RA4 (Distributed Socket-based): Compiled with `-O3` and `-Wall`, e.g., `gcc -O3 lab04.c -o lab04 -Wall`
* **Test Automation**: Lab directories contain automated testing bash scripts (e.g., `script.sh`, `run_ssh_tests.sh`, `run_local_tests.sh`). These scripts compile the code, orchestrate loops scaling input size (`n`) and thread/slave count (`t`), and append timing results to `results.csv`. Always use or adapt these `.sh` scripts when evaluating performance over multiple parameters.

## Architecture Guidelines
* **RA2/RA3 (Shared Memory)**: Utilizes `pthreads`. Be mindful of thread creation/join overhead, data partitioning, and thread safety.
* **RA4 (Distributed Memory via Sockets)**: Utilizes a custom TCP/IP Master-Slave architecture instead of standard MPI.
  * System config relies on `.txt` files (`config_master.txt`, `config_slaves.txt`) for dynamic setup (IPs, Ports). Do not hardcode these.
  * Testing uses SSH for deployment (`run_ssh_tests.sh`). Modify deployment scripts carefully if introducing dependencies.

## Key Files
* See the `README.md` in individual directories if available.
* Check [RA4/Swarm_Execution_Guide.md](RA4/Swarm_Execution_Guide.md) for specifics on distributed testing for RA4.