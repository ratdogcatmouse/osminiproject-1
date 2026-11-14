# osminiproject

A command-line prototype in C that demonstrates core operating-system concepts through a simulated university computing lab: CPU scheduling, thread synchronization, inter-process communication, deadlock avoidance, worker failure detection and resource limits.

## Features

| Command | Concept | What it shows |
|---|---|---|
| `fcfs`, `rr`, `priority`, `compare` | CPU scheduling | FCFS, Round Robin and non-preemptive Priority, with timelines, turnaround/waiting times and averages |
| `logger` | POSIX threads | Bounded producer-consumer buffer using a mutex and condition variables, writing to `campus.log` |
| `ipc` | Shared memory | Parent and child exchange data through POSIX shared memory, coordinated by named semaphores |
| `banker safe\|unsafe` | Deadlock avoidance | Banker's safety algorithm with a safe sequence and an unsafe-state test |
| `worker normal\|fail` | Process management | `fork()` / `waitpid()` workers, including a clearly labelled simulated failure |
| `limit` | Resource limits | `setrlimit(RLIMIT_CPU)` stopping a runaway process with SIGXCPU |
| `all` | Integration | Runs every module and prints a pass/fail summary |

## Prerequisites

- Ubuntu 20.04 or later, or WSL Ubuntu
- `gcc`, `make` and the pthreads library (all included in `build-essential`)

```bash
sudo apt update
sudo apt install -y build-essential
```

Check that the tools are installed:

```bash
gcc --version
make --version
```

## Getting the code

Clone the repository:

```bash
git clone https://github.com/ratdogcatmouse/osminiproject-1.git
cd osminiproject-1/src
```

## Building

From the folder that contains the `Makefile`:

```bash
make
```

This compiles each `.c` file with `-Wall -Wextra` and links the executable `campuscompute`. The build should finish with no warnings or errors.

## Running the program

Every feature is run as a separate command:

```bash
./campuscompute <command> [options]
```

Show all commands:

```bash
./campuscompute help
```

### 1. Job set

```bash
./campuscompute jobs              # list the built-in 5-job test set
./campuscompute jobs jobs.txt     # load and validate jobs from a file
```

Without a job file, a built-in set of 5 jobs is used:

| Job | Arrival | Burst | Priority |
|---|---|---|---|
| J1 | 0 | 5 | 3 |
| J2 | 1 | 3 | 1 |
| J3 | 2 | 8 | 4 |
| J4 | 3 | 6 | 1 |
| J5 | 4 | 4 | 2 |

**Job file format:** one job per line as `id arrival burst priority`. Lines starting with `#` are comments.

```
# id arrival burst priority
1 0 4 2
2 1 3 1
```

Limits: up to 20 jobs, id 1–999, arrival 0–1000, burst 1–100, priority 0–99 (a lower number means higher priority). Duplicate IDs are rejected.

### 2. CPU scheduling

```bash
./campuscompute fcfs                  # First-Come First-Served
./campuscompute rr                    # Round Robin, default quantum 2
./campuscompute rr 3                  # Round Robin with quantum 3
./campuscompute priority              # non-preemptive Priority
./campuscompute compare               # all three on the same jobs
./campuscompute compare 3 jobs.txt    # custom quantum and job file
```

Each scheduler prints its rule, an execution timeline, a per-job table (start, completion, turnaround, waiting, status) and the averages.

**Tie-breaking rules**
- **FCFS:** equal arrival → lower job ID first.
- **Round Robin:** FIFO ready queue; equal arrival → lower ID; jobs that arrive during a slice are queued before the preempted job.
- **Priority:** lower number = higher priority; ties → earlier arrival, then lower ID.

Expected averages for the built-in job set:

| Policy | Avg turnaround | Avg waiting |
|---|---|---|
| FCFS | 13.40 | 8.20 |
| Round Robin (q = 2) | 17.80 | 12.60 |
| Priority | 12.20 | 7.00 |

### 3. Producer-consumer logger (threads)

```bash
./campuscompute logger          # 2 producers x 5 events (default)
./campuscompute logger 3 3      # 3 producers x 3 events
```

Producers add events to a 4-slot shared buffer, and one consumer writes them to `campus.log`. Producers print `buffer full, waiting` when the buffer is full. At the end, all threads are joined and the mutex and condition variables are destroyed. Limits: 1–8 producers, 1–50 events each.

View the log:

```bash
cat campus.log
```

### 4. Shared-memory IPC

```bash
./campuscompute ipc
```

The parent writes the job bursts into POSIX shared memory. The child reads them, computes the total, longest and average burst, and writes the result back. Two named semaphores control the order. The shared memory (`/cc_shm_<pid>`) and semaphores (`/cc_req_<pid>`, `/cc_rep_<pid>`) are removed before the program exits.

### 5. Banker's algorithm

```bash
./campuscompute banker safe      # Available = (3 3 2)
./campuscompute banker unsafe    # Available = (0 1 1)
```

Prints the Available vector and the Allocation, Maximum and Need matrices (Need = Maximum − Allocation), then each step of the safety check.
- **Safe case:** safe sequence `P1 -> P3 -> P4 -> P0 -> P2`
- **Unsafe case:** `UNSAFE`, with P0, P2 and P4 unable to finish

### 6. Worker processes

```bash
./campuscompute worker normal    # both workers exit with status 0
./campuscompute worker fail      # worker 2 simulates a failure
```

The parent creates workers with `fork()` and collects their exit status with `waitpid()`. In `fail` mode, worker 2 deliberately exits with code 42. The output labels this as a **simulated** failure, not a real OS fault.

### 7. Resource limit

```bash
./campuscompute limit
```

A child process sets `RLIMIT_CPU` to 1 second (soft limit) and 2 seconds (hard limit), then runs an endless loop. The kernel sends `SIGXCPU` after about 1 second of CPU time, and the parent reports that the child was terminated. The "FAILED" status for this child is expected: the limit is supposed to stop it.

### 8. Run everything

```bash
./campuscompute all
```

Runs all 11 module tests in order and ends with a summary: `11/11 modules completed as expected.`

## Error handling

Invalid commands and malformed input print a clear error and exit with status 1:

```bash
./campuscompute rr abc               # quantum must be an integer from 1 to 100
./campuscompute jobs bad_jobs.txt    # bad_jobs.txt line 3: burst must be 1-100
./campuscompute banker maybe         # banker expects exactly one of: safe | unsafe
./campuscompute launch               # unknown command, followed by the usage text
```

## Cleanup

Remove the build files, the executable and the log:

```bash
make clean
```

The program removes every IPC object it creates and waits for all its threads and child processes before exiting. To confirm nothing is left behind:

```bash
ls /dev/shm | grep cc_                # no output expected
ps -ef | grep '[c]ampuscompute'       # no output expected
```

## Project structure

```
src/
├── main.c        command parsing, input validation, integrated run
├── jobs.c/.h     job model, test data, job-file validation
├── sched.c/.h    FCFS, Round Robin, Priority scheduling
├── logger.c/.h   producer-consumer thread logger
├── ipc.c/.h      shared memory and named semaphores
├── banker.c/.h   Banker's safety algorithm
├── worker.c/.h   worker processes and setrlimit demo
├── jobs.txt      sample job file
├── bad_jobs.txt  invalid job file for testing validation
└── Makefile
```

## Authors

Ishaan Bhatia and Jai Akash Mahendran
