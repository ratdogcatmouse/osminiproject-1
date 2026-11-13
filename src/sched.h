#ifndef SCHED_H
#define SCHED_H

#include "jobs.h"

#define DEFAULT_QUANTUM 2
#define MAX_QUANTUM     100

typedef enum { POLICY_FCFS, POLICY_RR, POLICY_PRIORITY } Policy;

int run_scheduler(Policy p, Job jobs[], int n, int quantum);
int compare_schedulers(const Job jobs[], int n, int quantum);

#endif
