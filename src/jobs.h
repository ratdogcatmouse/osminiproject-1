#ifndef JOBS_H
#define JOBS_H

#define MAX_JOBS     20
#define MAX_ARRIVAL  1000
#define MAX_BURST    100
#define MAX_PRIORITY 99

typedef enum { READY, RUNNING, DONE } JobStatus;

typedef struct {
    int id, arrival, burst, priority;
    int remaining, start, completion;
    JobStatus status;
} Job;

int  load_default_jobs(Job jobs[]);
int  load_jobs_from_file(const char *path, Job jobs[]);
void reset_jobs(Job jobs[], int n);
void print_jobs(const Job jobs[], int n);
const char *status_name(JobStatus s);

#endif
