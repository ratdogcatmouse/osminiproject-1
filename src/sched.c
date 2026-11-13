#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "sched.h"

/* Upper bound: every time unit of every burst as its own slice, plus idle gaps. */
#define MAX_SEGMENTS (MAX_JOBS * MAX_BURST + MAX_JOBS)

typedef struct { int job, start, end; } Segment;   /* job 0 = CPU idle */

typedef struct {
    Segment seg[MAX_SEGMENTS];
    int count;
} Timeline;

typedef struct { double avg_turnaround, avg_waiting; int makespan; } Summary;

static void add_segment(Timeline *t, int job, int start, int end)
{
    Segment *last = t->count ? &t->seg[t->count - 1] : NULL;
    if (last && last->job == job && last->end == start) {
        last->end = end;
        return;
    }
    t->seg[t->count++] = (Segment){ job, start, end };
}

static void idle_until(Timeline *t, int *clock, int until)
{
    if (until > *clock) {
        add_segment(t, 0, *clock, until);
        *clock = until;
    }
}

static void run_slice(Job *j, Timeline *t, int *clock, int slice)
{
    if (j->start < 0)
        j->start = *clock;
    j->status = RUNNING;
    add_segment(t, j->id, *clock, *clock + slice);
    *clock += slice;
    j->remaining -= slice;
    if (j->remaining == 0) {
        j->status = DONE;
        j->completion = *clock;
    } else {
        j->status = READY;
    }
}

/* Indices ordered by arrival time, ties by lower job id. */
static void sort_by_arrival(const Job jobs[], int n, int idx[])
{
    for (int i = 0; i < n; i++) {
        int k = i;
        while (k > 0 && (jobs[idx[k - 1]].arrival > jobs[i].arrival ||
                         (jobs[idx[k - 1]].arrival == jobs[i].arrival && jobs[idx[k - 1]].id > jobs[i].id))) {
            idx[k] = idx[k - 1];
            k--;
        }
        idx[k] = i;
    }
}

static void simulate_fcfs(Job jobs[], int n, Timeline *t)
{
    int idx[MAX_JOBS], clock = 0;
    sort_by_arrival(jobs, n, idx);
    for (int k = 0; k < n; k++) {
        Job *j = &jobs[idx[k]];
        idle_until(t, &clock, j->arrival);
        run_slice(j, t, &clock, j->burst);
    }
}

static void simulate_rr(Job jobs[], int n, int quantum, Timeline *t)
{
    int idx[MAX_JOBS], queue[MAX_JOBS];
    int head = 0, size = 0, next = 0, clock = 0, completed = 0;
    sort_by_arrival(jobs, n, idx);

#define ENQUEUE(i) (queue[(head + size++) % MAX_JOBS] = (i))

    while (completed < n) {
        while (next < n && jobs[idx[next]].arrival <= clock)
            ENQUEUE(idx[next++]);
        if (size == 0) {
            idle_until(t, &clock, jobs[idx[next]].arrival);
            continue;
        }

        Job *j = &jobs[queue[head]];
        head = (head + 1) % MAX_JOBS;
        size--;
        run_slice(j, t, &clock, j->remaining < quantum ? j->remaining : quantum);

        /* Jobs that arrived during this slice queue ahead of the preempted job. */
        while (next < n && jobs[idx[next]].arrival <= clock)
            ENQUEUE(idx[next++]);
        if (j->status == DONE)
            completed++;
        else
            ENQUEUE((int)(j - jobs));
    }
#undef ENQUEUE
}

static void simulate_priority(Job jobs[], int n, Timeline *t)
{
    int clock = 0, completed = 0;
    while (completed < n) {
        Job *best = NULL;
        int next_arrival = INT_MAX;
        for (int i = 0; i < n; i++) {
            Job *j = &jobs[i];
            if (j->status == DONE)
                continue;
            if (j->arrival > clock) {
                if (j->arrival < next_arrival)
                    next_arrival = j->arrival;
                continue;
            }
            if (!best || j->priority < best->priority ||
                (j->priority == best->priority &&
                 (j->arrival < best->arrival || (j->arrival == best->arrival && j->id < best->id))))
                best = j;
        }
        if (!best) {
            idle_until(t, &clock, next_arrival);
            continue;
        }
        run_slice(best, t, &clock, best->burst);
        completed++;
    }
}

static Summary simulate(Policy p, Job jobs[], int n, int quantum, Timeline *t)
{
    reset_jobs(jobs, n);
    t->count = 0;
    switch (p) {
    case POLICY_FCFS:     simulate_fcfs(jobs, n, t); break;
    case POLICY_RR:       simulate_rr(jobs, n, quantum, t); break;
    case POLICY_PRIORITY: simulate_priority(jobs, n, t); break;
    }

    Summary s = { 0 };
    for (int i = 0; i < n; i++) {
        int turnaround = jobs[i].completion - jobs[i].arrival;
        s.avg_turnaround += turnaround;
        s.avg_waiting += turnaround - jobs[i].burst;
    }
    s.avg_turnaround /= n;
    s.avg_waiting /= n;
    s.makespan = t->seg[t->count - 1].end;
    return s;
}

static const char *policy_name(Policy p)
{
    switch (p) {
    case POLICY_FCFS:     return "FCFS";
    case POLICY_RR:       return "Round Robin";
    case POLICY_PRIORITY: return "Priority (non-preemptive)";
    }
    return "?";
}

static void print_rule(Policy p, int quantum)
{
    switch (p) {
    case POLICY_FCFS:
        printf("Rule: run in arrival order; equal arrival -> lower job ID first.\n");
        break;
    case POLICY_RR:
        printf("Time quantum: %d\n", quantum);
        printf("Rule: FIFO ready queue; equal arrival -> lower job ID first;\n"
               "      jobs arriving during a slice are queued before the preempted job.\n");
        break;
    case POLICY_PRIORITY:
        printf("Rule: lower number = higher priority, no preemption;\n"
               "      ties -> earlier arrival, then lower job ID.\n");
        break;
    }
}

static void print_timeline(const Timeline *t)
{
    printf("\nExecution timeline:\n ");
    for (int i = 0; i < t->count; i++) {
        const Segment *s = &t->seg[i];
        if (s->job)
            printf(" [%3d-%3d] J%-3d", s->start, s->end, s->job);
        else
            printf(" [%3d-%3d] IDLE", s->start, s->end);
        if (i % 5 == 4 && i != t->count - 1)
            printf("\n ");
    }
    printf("\n");
}

static void print_results(const Job jobs[], int n, Summary s)
{
    printf("\n  Job  Arrival  Burst  Priority  Start  Completion  Turnaround  Waiting  Status\n");
    for (int i = 0; i < n; i++) {
        const Job *j = &jobs[i];
        int turnaround = j->completion - j->arrival;
        printf("  J%-3d %7d %6d %9d %6d %11d %11d %8d  %s\n", j->id, j->arrival, j->burst,
               j->priority, j->start, j->completion, turnaround, turnaround - j->burst,
               status_name(j->status));
    }
    printf("\n  Average turnaround time: %.2f\n", s.avg_turnaround);
    printf("  Average waiting time   : %.2f\n", s.avg_waiting);
}

int run_scheduler(Policy p, Job jobs[], int n, int quantum)
{
    static Timeline t;
    printf("=== %s scheduling (%d jobs) ===\n", policy_name(p), n);
    print_rule(p, quantum);
    Summary s = simulate(p, jobs, n, quantum, &t);
    print_timeline(&t);
    print_results(jobs, n, s);
    return 0;
}

int compare_schedulers(const Job jobs[], int n, int quantum)
{
    static Timeline t;
    const Policy policies[] = { POLICY_FCFS, POLICY_RR, POLICY_PRIORITY };
    Job copy[MAX_JOBS];
    Summary s[3];
    int best = 0;

    printf("=== Scheduling comparison (same %d jobs, RR quantum %d) ===\n\n", n, quantum);
    print_jobs(jobs, n);
    printf("\n  %-26s %15s %12s %9s\n", "Policy", "Avg Turnaround", "Avg Waiting", "Makespan");
    for (int k = 0; k < 3; k++) {
        memcpy(copy, jobs, n * sizeof *jobs);
        s[k] = simulate(policies[k], copy, n, quantum, &t);
        printf("  %-26s %15.2f %12.2f %9d\n", policy_name(policies[k]),
               s[k].avg_turnaround, s[k].avg_waiting, s[k].makespan);
        if (s[k].avg_waiting < s[best].avg_waiting)
            best = k;
    }
    printf("\nLowest average waiting time: %s (%.2f)\n", policy_name(policies[best]), s[best].avg_waiting);
    return 0;
}
