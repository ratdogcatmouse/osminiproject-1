#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "worker.h"

#define NUM_WORKERS       2
#define SIMULATED_EXIT    42

static void worker_task(int id, int work_units, int simulate_failure)
{
    printf("[worker %d] pid %d started, %d work units\n", id, getpid(), work_units);
    long result = 0;
    for (int u = 1; u <= work_units; u++) {
        result += (long)u * u;
        usleep(100000);
    }
    if (simulate_failure) {
        printf("[worker %d] SIMULATED FAILURE: exiting with code %d on purpose\n", id, SIMULATED_EXIT);
        fflush(stdout);
        _exit(SIMULATED_EXIT);
    }
    printf("[worker %d] finished work, result=%ld\n", id, result);
    fflush(stdout);
    _exit(0);
}

/* Prints how a child ended. Returns 1 for a normal exit with status 0. */
static int report_child(pid_t pid, int status, const char *who)
{
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        printf("[parent] %s (pid %d) exited with status %d -> %s\n", who, pid, code,
               code == 0 ? "COMPLETED" : "FAILED");
        return code == 0;
    }
    if (WIFSIGNALED(status))
        printf("[parent] %s (pid %d) terminated by signal %d (%s) -> FAILED\n", who, pid,
               WTERMSIG(status), strsignal(WTERMSIG(status)));
    return 0;
}

int run_worker_demo(int simulate_failure)
{
    pid_t pids[NUM_WORKERS];
    int started = 0, completed = 0, failed = 0;

    printf("=== Worker processes (%s) ===\n", simulate_failure ? "with simulated failure" : "normal run");
    for (; started < NUM_WORKERS; started++) {
        int id = started + 1;
        int fail = simulate_failure && id == NUM_WORKERS;
        fflush(stdout);
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            break;
        }
        if (pid == 0)
            worker_task(id, 2 + 2 * started, fail);
        pids[started] = pid;
        printf("[parent] forked worker %d (pid %d)%s\n", id, pid, fail ? " - will simulate a failure" : "");
    }

    for (int k = 0; k < started; k++) {
        int status;
        pid_t pid = waitpid(-1, &status, 0);
        if (pid < 0) {
            perror("waitpid");
            return -1;
        }
        char who[16] = "worker ?";
        for (int i = 0; i < started; i++)
            if (pids[i] == pid)
                snprintf(who, sizeof who, "worker %d", i + 1);
        if (report_child(pid, status, who))
            completed++;
        else
            failed++;
    }

    printf("\nSummary: %d completed, %d failed%s\n", completed, failed,
           simulate_failure ? " (the failure was simulated, not a real OS fault)" : "");
    return started == NUM_WORKERS && failed == (simulate_failure ? 1 : 0) ? 0 : -1;
}

int run_limit_demo(void)
{
    printf("=== Resource limit: RLIMIT_CPU ===\n");
    fflush(stdout);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid == 0) {
        struct rlimit lim = { .rlim_cur = 1, .rlim_max = 2 };
        if (setrlimit(RLIMIT_CPU, &lim) < 0) {
            perror("setrlimit");
            _exit(1);
        }
        getrlimit(RLIMIT_CPU, &lim);
        printf("[child %d] RLIMIT_CPU soft=%lu s hard=%lu s; starting endless CPU-bound loop\n",
               getpid(), (unsigned long)lim.rlim_cur, (unsigned long)lim.rlim_max);
        fflush(stdout);
        for (volatile unsigned long x = 0;; x++)
            ;
    }

    int status;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return -1;
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;

    report_child(pid, status, "limited child");
    if (WIFSIGNALED(status) && WTERMSIG(status) == SIGXCPU) {
        printf("Observed effect: the kernel sent SIGXCPU once the child used its 1 s CPU allowance,\n"
               "stopping the runaway loop after %.2f s of wall time.\n", elapsed);
        return 0;
    }
    if (WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL) {
        printf("Observed effect: the child reached the 2 s hard limit and was killed (SIGKILL).\n");
        return 0;
    }
    fprintf(stderr, "Unexpected: the CPU limit did not stop the child\n");
    return -1;
}
