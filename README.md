# osminiproject-1

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

## Build and run

Requires Ubuntu or WSL Ubuntu with `gcc` and `make` (`sudo apt install build-essential`).

```bash
cd src
make
./campuscompute help
./campuscompute all
make clean
```

See [`src/README.md`](src/README.md) for every command and option.

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
└── Makefile
```

## Authors

Ishaan Bhatia and Jai Akash Mahendran

