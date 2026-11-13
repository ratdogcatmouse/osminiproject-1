# CampusCompute

Command-line prototype for job scheduling, threaded logging, shared-memory IPC,
Banker's safety check, worker failure detection and resource limits.

## Prerequisites

Ubuntu or WSL Ubuntu with `gcc` and `make`:

    sudo apt install build-essential

## Build

    make

## Run

    ./campuscompute help
    ./campuscompute jobs [jobfile]
    ./campuscompute fcfs [jobfile]
    ./campuscompute rr [quantum [jobfile]]       # default quantum 2
    ./campuscompute priority [jobfile]
    ./campuscompute compare [quantum [jobfile]]
    ./campuscompute logger [producers [events]]  # default 2 x 5, writes campus.log
    ./campuscompute ipc
    ./campuscompute banker safe | unsafe
    ./campuscompute worker normal | fail
    ./campuscompute limit
    ./campuscompute all

With no jobfile, a built-in 5-job test set is used. `jobs.txt` is a sample job
file and `bad_jobs.txt` is a deliberately invalid one for testing validation.

Job file format: one job per line, `id arrival burst priority`; lines starting
with `#` are comments. Limits: up to 20 jobs, id 1-999, arrival 0-1000,
burst 1-100, priority 0-99 (lower number = higher priority).

## Cleanup

    make clean      # removes object files, the executable and campus.log

The program removes its own shared memory and semaphores (`/cc_*_<pid>`) and
waits for every thread and child process before exiting. To confirm:

    ls /dev/shm | grep cc_      # no output expected
    pgrep campuscompute         # no output expected
