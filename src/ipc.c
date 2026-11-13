#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include "ipc.h"
#include "jobs.h"

typedef struct {
    int count;
    int bursts[MAX_JOBS];
    int total, longest;
    double average;
    pid_t worker_pid;
} SharedData;

static void child_process(SharedData *data, sem_t *request, sem_t *reply)
{
    if (sem_wait(request) < 0) {
        perror("child sem_wait");
        _exit(1);
    }
    data->total = data->longest = 0;
    for (int i = 0; i < data->count; i++) {
        data->total += data->bursts[i];
        if (data->bursts[i] > data->longest)
            data->longest = data->bursts[i];
    }
    data->average = (double)data->total / data->count;
    data->worker_pid = getpid();
    printf("[child  %d] read %d bursts, wrote back total=%d longest=%d average=%.2f\n",
           getpid(), data->count, data->total, data->longest, data->average);
    fflush(stdout);
    sem_post(reply);
    _exit(0);
}

static void close_semaphore(sem_t *sem, const char *name)
{
    if (sem == SEM_FAILED)
        return;
    sem_close(sem);
    sem_unlink(name);
    sem_t *check = sem_open(name, 0);
    if (check == SEM_FAILED && errno == ENOENT)
        printf("  semaphore %s removed\n", name);
    else if (check != SEM_FAILED)
        sem_close(check);
}

int run_ipc_demo(void)
{
    char shm_name[32], req_name[32], rep_name[32];
    snprintf(shm_name, sizeof shm_name, "/cc_shm_%d", getpid());
    snprintf(req_name, sizeof req_name, "/cc_req_%d", getpid());
    snprintf(rep_name, sizeof rep_name, "/cc_rep_%d", getpid());

    int fd, rc = -1;
    SharedData *data = MAP_FAILED;
    sem_t *request = SEM_FAILED, *reply = SEM_FAILED;

    printf("=== Shared-memory IPC ===\n");
    fd = shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0) {
        perror("shm_open");
        return -1;
    }
    if (ftruncate(fd, sizeof *data) < 0) {
        perror("ftruncate");
        goto cleanup;
    }
    data = mmap(NULL, sizeof *data, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        goto cleanup;
    }
    request = sem_open(req_name, O_CREAT | O_EXCL, 0600, 0);
    reply = sem_open(rep_name, O_CREAT | O_EXCL, 0600, 0);
    if (request == SEM_FAILED || reply == SEM_FAILED) {
        perror("sem_open");
        goto cleanup;
    }
    printf("Created shared memory %s (%zu bytes) and semaphores %s, %s\n\n",
           shm_name, sizeof *data, req_name, rep_name);

    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        goto cleanup;
    }
    if (pid == 0)
        child_process(data, request, reply);

    Job jobs[MAX_JOBS];
    data->count = load_default_jobs(jobs);
    printf("[parent %d] wrote CPU bursts to shared memory:", getpid());
    for (int i = 0; i < data->count; i++) {
        data->bursts[i] = jobs[i].burst;
        printf(" %d", data->bursts[i]);
    }
    printf("\n[parent %d] sem_post(%s), waiting on %s\n", getpid(), req_name, rep_name);
    fflush(stdout);
    sem_post(request);

    while (sem_wait(reply) < 0 && errno == EINTR)
        ;
    printf("[parent %d] read result from child %d: total=%d longest=%d average=%.2f\n",
           getpid(), data->worker_pid, data->total, data->longest, data->average);

    int status;
    if (waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        printf("[parent %d] child %d exited normally\n", getpid(), pid);
        rc = 0;
    } else {
        fprintf(stderr, "Error: IPC child did not exit cleanly\n");
    }

cleanup:
    printf("\nCleanup:\n");
    if (data != MAP_FAILED)
        munmap(data, sizeof *data);
    close(fd);
    shm_unlink(shm_name);
    if (shm_open(shm_name, O_RDONLY, 0) < 0 && errno == ENOENT)
        printf("  shared memory %s removed\n", shm_name);
    close_semaphore(request, req_name);
    close_semaphore(reply, rep_name);
    return rc;
}
