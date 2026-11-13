#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "banker.h"
#include "ipc.h"
#include "jobs.h"
#include "logger.h"
#include "sched.h"
#include "worker.h"

static void usage(void)
{
    puts("Usage: ./campuscompute <command> [options]\n"
         "\n"
         "Commands:\n"
         "  help                          show this message\n"
         "  jobs [jobfile]                list and validate the job set\n"
         "  fcfs [jobfile]                First-Come First-Served scheduling\n"
         "  rr [quantum [jobfile]]        Round Robin (default quantum 2)\n"
         "  priority [jobfile]            non-preemptive Priority scheduling\n"
         "  compare [quantum [jobfile]]   compare all three policies on the same jobs\n"
         "  logger [producers [events]]   producer-consumer thread logger (default 2 x 5)\n"
         "  ipc                           shared-memory IPC with named semaphores\n"
         "  banker safe|unsafe            Banker's safety algorithm test case\n"
         "  worker normal|fail            fork/waitpid workers (fail = simulated failure)\n"
         "  limit                         setrlimit(RLIMIT_CPU) demonstration\n"
         "  all                           run every module and print a summary\n"
         "\n"
         "Without a jobfile the built-in 5-job test set is used.\n"
         "Jobfile format: one job per line as \"id arrival burst priority\"; '#' lines are comments.");
}

static int usage_error(const char *msg)
{
    fprintf(stderr, "Error: %s\nRun './campuscompute help' for usage.\n", msg);
    return -1;
}

static int parse_int(const char *s, int min, int max, const char *what, int *out)
{
    char *end;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno || end == s || *end || v < min || v > max) {
        fprintf(stderr, "Error: %s must be an integer from %d to %d (got '%s')\n", what, min, max, s);
        return -1;
    }
    *out = (int)v;
    return 0;
}

static int get_jobs(const char *path, Job jobs[])
{
    return path ? load_jobs_from_file(path, jobs) : load_default_jobs(jobs);
}

static int cmd_jobs(int argc, char **argv)
{
    if (argc > 3)
        return usage_error("jobs takes at most one argument: [jobfile]");
    Job jobs[MAX_JOBS];
    int n = get_jobs(argc == 3 ? argv[2] : NULL, jobs);
    if (n < 0)
        return -1;
    print_jobs(jobs, n);
    printf("%d job(s) loaded and validated.\n", n);
    return 0;
}

/* policy < 0 means "compare all policies". */
static int cmd_schedule(int argc, char **argv, int policy)
{
    int takes_quantum = policy == POLICY_RR || policy < 0;
    int quantum = DEFAULT_QUANTUM, file_arg = takes_quantum ? 3 : 2;

    if (argc > file_arg + 1)
        return usage_error(takes_quantum ? "expected: [quantum [jobfile]]" : "expected: [jobfile]");
    if (takes_quantum && argc >= 3 && parse_int(argv[2], 1, MAX_QUANTUM, "quantum", &quantum) < 0)
        return -1;

    Job jobs[MAX_JOBS];
    int n = get_jobs(argc > file_arg ? argv[file_arg] : NULL, jobs);
    if (n < 0)
        return -1;
    return policy < 0 ? compare_schedulers(jobs, n, quantum) : run_scheduler(policy, jobs, n, quantum);
}

static int cmd_logger(int argc, char **argv)
{
    int producers = 2, events = 5;
    if (argc > 4)
        return usage_error("logger expects: [producers [events]]");
    if (argc >= 3 && parse_int(argv[2], 1, MAX_PRODUCERS, "producers", &producers) < 0)
        return -1;
    if (argc >= 4 && parse_int(argv[3], 1, MAX_EVENTS, "events", &events) < 0)
        return -1;
    return run_logger_demo(producers, events);
}

/* Returns 0 if argv[2] is a, 1 if it is b, or -1 with an error message. */
static int pick_option(int argc, char **argv, const char *a, const char *b)
{
    if (argc != 3 || (strcmp(argv[2], a) && strcmp(argv[2], b))) {
        fprintf(stderr, "Error: %s expects exactly one of: %s | %s\n", argv[1], a, b);
        return -1;
    }
    return strcmp(argv[2], a) == 0 ? 0 : 1;
}

static int run_all(void)
{
    struct { const char *name; int rc; } results[11];
    int count = 0, passed = 0;
    Job jobs[MAX_JOBS];
    int n = load_default_jobs(jobs);

#define STEP(label, call)                                         \
    do {                                                          \
        printf("\n==================== %s ====================\n", label); \
        results[count].name = label;                              \
        results[count++].rc = (call);                             \
    } while (0)

    STEP("FCFS scheduling", run_scheduler(POLICY_FCFS, jobs, n, DEFAULT_QUANTUM));
    STEP("Round Robin scheduling", run_scheduler(POLICY_RR, jobs, n, DEFAULT_QUANTUM));
    STEP("Priority scheduling", run_scheduler(POLICY_PRIORITY, jobs, n, DEFAULT_QUANTUM));
    STEP("Scheduling comparison", compare_schedulers(jobs, n, DEFAULT_QUANTUM));
    STEP("Producer-consumer logger", run_logger_demo(2, 5));
    STEP("Shared-memory IPC", run_ipc_demo());
    STEP("Banker safe case", run_banker_demo(0));
    STEP("Banker unsafe case", run_banker_demo(1));
    STEP("Workers (normal)", run_worker_demo(0));
    STEP("Workers (simulated failure)", run_worker_demo(1));
    STEP("Resource limit", run_limit_demo());
#undef STEP

    printf("\n==================== SUMMARY ====================\n");
    for (int i = 0; i < count; i++) {
        printf("  %-30s %s\n", results[i].name, results[i].rc == 0 ? "OK" : "FAILED");
        passed += results[i].rc == 0;
    }
    printf("\n%d/%d modules completed as expected.\n", passed, count);
    return passed == count ? 0 : -1;
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IOLBF, 0);   /* keep parent/child output in order when piped */
    if (argc < 2) {
        usage();
        return 1;
    }

    const char *cmd = argv[1];
    int rc, opt;

    if (!strcmp(cmd, "help")) {
        usage();
        return 0;
    }
    if (!strcmp(cmd, "jobs"))
        rc = cmd_jobs(argc, argv);
    else if (!strcmp(cmd, "fcfs"))
        rc = cmd_schedule(argc, argv, POLICY_FCFS);
    else if (!strcmp(cmd, "rr"))
        rc = cmd_schedule(argc, argv, POLICY_RR);
    else if (!strcmp(cmd, "priority"))
        rc = cmd_schedule(argc, argv, POLICY_PRIORITY);
    else if (!strcmp(cmd, "compare"))
        rc = cmd_schedule(argc, argv, -1);
    else if (!strcmp(cmd, "logger"))
        rc = cmd_logger(argc, argv);
    else if (!strcmp(cmd, "ipc"))
        rc = argc == 2 ? run_ipc_demo() : usage_error("ipc takes no arguments");
    else if (!strcmp(cmd, "banker"))
        rc = (opt = pick_option(argc, argv, "safe", "unsafe")) < 0 ? -1 : run_banker_demo(opt);
    else if (!strcmp(cmd, "worker"))
        rc = (opt = pick_option(argc, argv, "normal", "fail")) < 0 ? -1 : run_worker_demo(opt);
    else if (!strcmp(cmd, "limit"))
        rc = argc == 2 ? run_limit_demo() : usage_error("limit takes no arguments");
    else if (!strcmp(cmd, "all"))
        rc = argc == 2 ? run_all() : usage_error("all takes no arguments");
    else {
        fprintf(stderr, "Error: unknown command '%s'\n\n", cmd);
        usage();
        return 1;
    }
    return rc == 0 ? 0 : 1;
}
