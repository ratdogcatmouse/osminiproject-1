#include <stdio.h>
#include <string.h>
#include "jobs.h"

static const Job DEFAULT_JOBS[] = {
    { .id = 1, .arrival = 0, .burst = 5, .priority = 3 },
    { .id = 2, .arrival = 1, .burst = 3, .priority = 1 },
    { .id = 3, .arrival = 2, .burst = 8, .priority = 4 },
    { .id = 4, .arrival = 3, .burst = 6, .priority = 1 },
    { .id = 5, .arrival = 4, .burst = 4, .priority = 2 },
};

const char *status_name(JobStatus s)
{
    switch (s) {
    case READY:   return "READY";
    case RUNNING: return "RUNNING";
    case DONE:    return "DONE";
    }
    return "?";
}

void reset_jobs(Job jobs[], int n)
{
    for (int i = 0; i < n; i++) {
        jobs[i].remaining  = jobs[i].burst;
        jobs[i].start      = -1;
        jobs[i].completion = 0;
        jobs[i].status     = READY;
    }
}

int load_default_jobs(Job jobs[])
{
    int n = sizeof DEFAULT_JOBS / sizeof DEFAULT_JOBS[0];
    memcpy(jobs, DEFAULT_JOBS, sizeof DEFAULT_JOBS);
    reset_jobs(jobs, n);
    return n;
}

static const char *validate_job(const Job *j, const Job jobs[], int n)
{
    if (j->id < 1 || j->id > 999)                       return "id must be 1-999";
    if (j->arrival < 0 || j->arrival > MAX_ARRIVAL)     return "arrival must be 0-1000";
    if (j->burst < 1 || j->burst > MAX_BURST)           return "burst must be 1-100";
    if (j->priority < 0 || j->priority > MAX_PRIORITY)  return "priority must be 0-99";
    for (int i = 0; i < n; i++)
        if (jobs[i].id == j->id)                        return "duplicate job id";
    return NULL;
}

/* File format: one job per line, "id arrival burst priority". '#' starts a comment line. */
int load_jobs_from_file(const char *path, Job jobs[])
{
    FILE *f = fopen(path, "r");
    if (!f) {
        perror(path);
        return -1;
    }

    char buf[256];
    int n = 0, line = 0;
    while (fgets(buf, sizeof buf, f)) {
        line++;
        char *p = buf + strspn(buf, " \t\r\n");
        if (*p == '\0' || *p == '#')
            continue;

        Job j = {0};
        int used = 0;
        const char *err = NULL;
        if (sscanf(p, "%d %d %d %d %n", &j.id, &j.arrival, &j.burst, &j.priority, &used) != 4
            || p[used] != '\0')
            err = "expected 4 integers: id arrival burst priority";
        else if (n == MAX_JOBS)
            err = "too many jobs (max 20)";
        else
            err = validate_job(&j, jobs, n);

        if (err) {
            fprintf(stderr, "Error: %s line %d: %s\n", path, line, err);
            fclose(f);
            return -1;
        }
        jobs[n++] = j;
    }
    fclose(f);

    if (n == 0) {
        fprintf(stderr, "Error: %s contains no jobs\n", path);
        return -1;
    }
    reset_jobs(jobs, n);
    return n;
}

void print_jobs(const Job jobs[], int n)
{
    printf("  Job  Arrival  Burst  Priority  Status\n");
    for (int i = 0; i < n; i++)
        printf("  J%-3d %7d %6d %9d  %s\n", jobs[i].id, jobs[i].arrival, jobs[i].burst,
               jobs[i].priority, status_name(jobs[i].status));
}
