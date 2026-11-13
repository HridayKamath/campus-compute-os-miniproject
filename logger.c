/*
 * logger.c - FR3: concurrent activity logger
 *
 * Bounded producer-consumer queue using POSIX threads.
 *   - 3 producer threads put event messages into a queue of size QSIZE
 *   - 1 consumer (logger) thread takes them out and writes them to a log file
 *   - a mutex protects the queue; two condition variables handle the
 *     "queue full" (producers wait) and "queue empty" (consumer waits) cases
 *   - the consumer is deliberately slower so the queue really fills up
 */
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "campuscompute.h"

#define QSIZE              5
#define MSG_LEN            96
#define NUM_PRODUCERS      3
#define MSGS_PER_PRODUCER  4

static char queue[QSIZE][MSG_LEN];
static int  q_head, q_tail, q_count;
static int  producers_finished;
static int  total_logged;
static FILE *logfp;

static pthread_mutex_t lock      = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  not_full  = PTHREAD_COND_INITIALIZER;
static pthread_cond_t  not_empty = PTHREAD_COND_INITIALIZER;

static const char *events[] = {
    "Job submitted to queue",
    "Job started on CPU",
    "Job completed",
    "Resource request granted"
};

static void *producer(void *arg)
{
    int id = (int)(long)arg;

    for (int k = 0; k < MSGS_PER_PRODUCER; k++) {
        char msg[MSG_LEN];
        snprintf(msg, sizeof msg, "Producer-%d event %d: %s",
                 id, k + 1, events[(id + k) % 4]);

        pthread_mutex_lock(&lock);
        while (q_count == QSIZE) {                       /* queue FULL */
            printf("[PRODUCER %d] queue FULL -> waiting\n", id);
            pthread_cond_wait(&not_full, &lock);
        }
        strcpy(queue[q_tail], msg);
        q_tail = (q_tail + 1) % QSIZE;
        q_count++;
        printf("[PRODUCER %d] queued  : %s   (queue size = %d)\n", id, msg, q_count);
        pthread_cond_signal(&not_empty);
        pthread_mutex_unlock(&lock);

        usleep(40000 * id);                              /* simulate work */
    }

    pthread_mutex_lock(&lock);
    producers_finished++;
    pthread_cond_broadcast(&not_empty);   /* wake consumer so it can exit */
    pthread_mutex_unlock(&lock);
    return NULL;
}

static void *consumer(void *arg)
{
    (void)arg;
    for (;;) {
        char msg[MSG_LEN];

        pthread_mutex_lock(&lock);
        while (q_count == 0 && producers_finished < NUM_PRODUCERS) {
            printf("[CONSUMER  ] queue EMPTY -> waiting\n");
            pthread_cond_wait(&not_empty, &lock);
        }
        if (q_count == 0 && producers_finished == NUM_PRODUCERS) {
            pthread_mutex_unlock(&lock);                 /* all done */
            break;
        }
        strcpy(msg, queue[q_head]);
        q_head = (q_head + 1) % QSIZE;
        q_count--;
        pthread_cond_signal(&not_full);
        pthread_mutex_unlock(&lock);

        /* write to the log file outside the lock (only this thread writes) */
        char stamp[32];
        time_t now = time(NULL);
        strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", localtime(&now));
        total_logged++;
        fprintf(logfp, "[%s] [Entry %02d] %s\n", stamp, total_logged, msg);
        fflush(logfp);
        printf("[CONSUMER  ] logged   : %s\n", msg);

        usleep(150000);                                  /* slow consumer */
    }
    return NULL;
}

int run_logger_demo(void)
{
    pthread_t prod[NUM_PRODUCERS], cons;
    int rc = 0;

    printf("\n===== Concurrent Activity Logger (producer-consumer) =====\n");
    printf("Queue size = %d, producers = %d, messages per producer = %d\n\n",
           QSIZE, NUM_PRODUCERS, MSGS_PER_PRODUCER);

    q_head = q_tail = q_count = 0;
    producers_finished = 0;
    total_logged = 0;

    logfp = fopen(LOG_FILE, "w");
    if (!logfp) {
        fprintf(stderr, "Error: cannot create log file '%s': %s\n",
                LOG_FILE, strerror(errno));
        return -1;
    }

    if (pthread_create(&cons, NULL, consumer, NULL) != 0) {
        fprintf(stderr, "Error: pthread_create (consumer) failed\n");
        fclose(logfp);
        return -1;
    }
    int created = 0;
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        if (pthread_create(&prod[i], NULL, producer, (void *)(long)(i + 1)) != 0) {
            fprintf(stderr, "Error: pthread_create (producer %d) failed\n", i + 1);
            rc = -1;
            break;
        }
        created++;
    }
    if (rc != 0) {
        /* could not start every producer: pretend the missing ones finished */
        pthread_mutex_lock(&lock);
        producers_finished += NUM_PRODUCERS - created;
        pthread_cond_broadcast(&not_empty);
        pthread_mutex_unlock(&lock);
    }

    /* orderly termination: join every thread we created */
    for (int i = 0; i < created; i++) {
        pthread_join(prod[i], NULL);
        printf("[MAIN] producer thread %d joined\n", i + 1);
    }
    pthread_join(cons, NULL);
    printf("[MAIN] consumer thread joined\n");

    fclose(logfp);
    logfp = NULL;

    printf("\nProduced %d messages, logged %d messages -> %s\n",
           created * MSGS_PER_PRODUCER, total_logged, LOG_FILE);
    printf("All logger threads terminated cleanly.\n");
    return rc;
}
