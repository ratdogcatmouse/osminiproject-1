#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "logger.h"

#define BUF_SIZE 4
#define MSG_LEN  96

typedef struct {
    char items[BUF_SIZE][MSG_LEN];
    int head, tail, count;
    int producers_left;
    pthread_mutex_t lock;
    pthread_cond_t not_full, not_empty;
} LogBuffer;

typedef struct { LogBuffer *buf; int id, events; } ProducerArg;
typedef struct { LogBuffer *buf; FILE *log; int written; } ConsumerArg;

static void timestamp(char *out, size_t len)
{
    struct timespec ts;
    struct tm tm;
    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tm);
    size_t k = strftime(out, len, "%Y-%m-%d %H:%M:%S", &tm);
    snprintf(out + k, len - k, ".%03ld", ts.tv_nsec / 1000000);
}

static void *producer(void *arg)
{
    ProducerArg *a = arg;
    LogBuffer *b = a->buf;

    for (int e = 1; e <= a->events; e++) {
        pthread_mutex_lock(&b->lock);
        while (b->count == BUF_SIZE) {
            printf("[producer %d] buffer full, waiting\n", a->id);
            pthread_cond_wait(&b->not_full, &b->lock);
        }
        snprintf(b->items[b->tail], MSG_LEN, "producer %d: job event %d submitted", a->id, e);
        b->tail = (b->tail + 1) % BUF_SIZE;
        b->count++;
        printf("[producer %d] queued event %d   (buffer %d/%d)\n", a->id, e, b->count, BUF_SIZE);
        pthread_cond_signal(&b->not_empty);
        pthread_mutex_unlock(&b->lock);
        usleep(5000);
    }

    pthread_mutex_lock(&b->lock);
    b->producers_left--;
    printf("[producer %d] finished\n", a->id);
    pthread_cond_broadcast(&b->not_empty);
    pthread_mutex_unlock(&b->lock);
    return NULL;
}

static void *consumer(void *arg)
{
    ConsumerArg *a = arg;
    LogBuffer *b = a->buf;
    char msg[MSG_LEN], ts[32];

    for (;;) {
        pthread_mutex_lock(&b->lock);
        while (b->count == 0 && b->producers_left > 0)
            pthread_cond_wait(&b->not_empty, &b->lock);
        if (b->count == 0) {
            pthread_mutex_unlock(&b->lock);
            break;
        }
        memcpy(msg, b->items[b->head], MSG_LEN);
        b->head = (b->head + 1) % BUF_SIZE;
        b->count--;
        pthread_cond_signal(&b->not_full);
        pthread_mutex_unlock(&b->lock);

        timestamp(ts, sizeof ts);
        fprintf(a->log, "[%s] %s\n", ts, msg);
        a->written++;
        printf("[consumer]   logged: %s\n", msg);
        usleep(20000);   /* slower than producers so the full-buffer wait is visible */
    }
    printf("[consumer]   buffer empty and all producers done, exiting\n");
    return NULL;
}

int run_logger_demo(int producers, int events_each)
{
    FILE *log = fopen(LOG_FILE, "a");
    if (!log) {
        perror(LOG_FILE);
        return -1;
    }

    LogBuffer b = { .producers_left = producers };
    pthread_mutex_init(&b.lock, NULL);
    pthread_cond_init(&b.not_full, NULL);
    pthread_cond_init(&b.not_empty, NULL);

    ConsumerArg ca = { &b, log, 0 };
    ProducerArg pa[MAX_PRODUCERS];
    pthread_t pt[MAX_PRODUCERS], ct;
    int started = 0, rc = 0;
    char ts[32];

    timestamp(ts, sizeof ts);
    fprintf(log, "[%s] --- logger session start: %d producer(s) x %d event(s) ---\n", ts, producers, events_each);
    printf("=== Producer-consumer logger ===\n");
    printf("Bounded buffer: %d slots | %d producer(s) x %d event(s) | 1 consumer | log: %s\n\n",
           BUF_SIZE, producers, events_each, LOG_FILE);

    if (pthread_create(&ct, NULL, consumer, &ca) != 0) {
        fprintf(stderr, "Error: could not create consumer thread\n");
        rc = -1;
        goto cleanup;
    }
    for (; started < producers; started++) {
        pa[started] = (ProducerArg){ &b, started + 1, events_each };
        if (pthread_create(&pt[started], NULL, producer, &pa[started]) != 0) {
            fprintf(stderr, "Error: could not create producer thread %d\n", started + 1);
            rc = -1;
            break;
        }
    }
    if (started < producers) {
        /* Let the consumer exit even though not every producer started. */
        pthread_mutex_lock(&b.lock);
        b.producers_left -= producers - started;
        pthread_cond_broadcast(&b.not_empty);
        pthread_mutex_unlock(&b.lock);
    }

    for (int i = 0; i < started; i++)
        pthread_join(pt[i], NULL);
    pthread_join(ct, NULL);
    printf("\nAll %d thread(s) joined. %d event(s) written to %s\n", started + 1, ca.written, LOG_FILE);

cleanup:
    timestamp(ts, sizeof ts);
    fprintf(log, "[%s] --- logger session end: %d event(s) written ---\n", ts, ca.written);
    fclose(log);
    pthread_cond_destroy(&b.not_empty);
    pthread_cond_destroy(&b.not_full);
    pthread_mutex_destroy(&b.lock);
    printf("Mutex and condition variables destroyed, log file closed.\n");
    return rc;
}
