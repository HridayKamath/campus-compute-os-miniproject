/*
 * ipc.c - FR4: POSIX shared memory + named semaphores between two processes
 *
 *   Parent (producer): writes job burst times into shared memory, then posts
 *                      the "data ready" semaphore.
 *   Child  (consumer): waits on "data ready", computes total/average/max load,
 *                      writes the answer back to shared memory and posts the
 *                      "result ready" semaphore.
 *   Parent then reads the result, verifies it, waits for the child and
 *   removes the shared memory object and both semaphores it created.
 */
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "campuscompute.h"

typedef struct {
    int    count;
    int    job_id[MAX_JOBS];
    int    burst[MAX_JOBS];
    long   total_burst;      /* written by the child */
    double avg_burst;        /* written by the child */
    int    max_burst;        /* written by the child */
} SharedData;

/* sem_wait that retries when interrupted by a signal */
static int sem_wait_retry(sem_t *s)
{
    int r;
    while ((r = sem_wait(s)) == -1 && errno == EINTR) { }
    return r;
}

static int remove_one_shm(const char *name, int verbose)
{
    if (shm_unlink(name) == 0) {
        if (verbose) printf("  removed shared memory  %s\n", name);
    } else if (errno == ENOENT) {
        if (verbose) printf("  shared memory %s : not present\n", name);
    } else {
        fprintf(stderr, "  shm_unlink(%s): %s\n", name, strerror(errno));
        return -1;
    }
    return 0;
}

static int remove_one_sem(const char *name, int verbose)
{
    if (sem_unlink(name) == 0) {
        if (verbose) printf("  removed semaphore      %s\n", name);
    } else if (errno == ENOENT) {
        if (verbose) printf("  semaphore %s : not present\n", name);
    } else {
        fprintf(stderr, "  sem_unlink(%s): %s\n", name, strerror(errno));
        return -1;
    }
    return 0;
}

/* Removes ONLY the IPC objects created by this program, then reports whether
 * anything with our names is still present under /dev/shm. */
int cleanup_ipc(int verbose)
{
    int rc = 0;
    if (verbose) printf("\n===== IPC cleanup =====\n");
    rc |= remove_one_shm(SHM_NAME, verbose);
    rc |= remove_one_sem(SEM_DATA_NAME, verbose);
    rc |= remove_one_sem(SEM_RESULT_NAME, verbose);

    if (verbose) {
        int left = 0;
        if (access("/dev/shm/campuscompute_shm", F_OK) == 0)            left++;
        if (access("/dev/shm/sem.campuscompute_sem_data", F_OK) == 0)   left++;
        if (access("/dev/shm/sem.campuscompute_sem_result", F_OK) == 0) left++;
        printf("Remaining CampusCompute IPC objects in /dev/shm: %d\n", left);
        if (left == 0) printf("Cleanup verified: no program-created IPC objects remain.\n");
    }
    return rc ? -1 : 0;
}

/* Child process: the consumer */
static void child_consumer(SharedData *sh, sem_t *sem_data, sem_t *sem_result)
{
    printf("[CHILD  pid=%d] waiting on semaphore '%s' ...\n", getpid(), SEM_DATA_NAME);
    fflush(stdout);
    if (sem_wait_retry(sem_data) == -1) {
        perror("child sem_wait");
        _exit(1);
    }
    printf("[CHILD  pid=%d] semaphore acquired, reading %d jobs from shared memory\n",
           getpid(), sh->count);

    long total = 0;
    int max = 0;
    for (int i = 0; i < sh->count; i++) {
        total += sh->burst[i];
        if (sh->burst[i] > max) max = sh->burst[i];
    }
    sh->total_burst = total;
    sh->avg_burst   = sh->count ? (double)total / sh->count : 0.0;
    sh->max_burst   = max;
    printf("[CHILD  pid=%d] computed total=%ld avg=%.2f max=%d, writing result back\n",
           getpid(), total, sh->avg_burst, max);
    fflush(stdout);

    sem_post(sem_result);                    /* tell parent the result is ready */
    munmap(sh, sizeof(SharedData));
    sem_close(sem_data);
    sem_close(sem_result);
    _exit(0);
}

int run_ipc_demo(const char *jobfile)
{
    Job jobs[MAX_JOBS];
    int n = load_jobs(jobfile, jobs, MAX_JOBS);
    if (n < 0) return -1;

    printf("\n===== Shared-memory IPC with named semaphores =====\n");

    /* remove leftovers of OUR OWN objects from an earlier crashed run */
    cleanup_ipc(0);

    int rc = -1, fd = -1;
    SharedData *sh = MAP_FAILED;
    sem_t *sem_data = SEM_FAILED, *sem_result = SEM_FAILED;
    pid_t pid = -1;

    fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd == -1) { perror("shm_open"); goto out; }
    printf("[PARENT pid=%d] created shared memory %s\n", getpid(), SHM_NAME);

    if (ftruncate(fd, sizeof(SharedData)) == -1) { perror("ftruncate"); goto out; }
    sh = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (sh == MAP_FAILED) { perror("mmap"); goto out; }
    memset(sh, 0, sizeof(SharedData));

    sem_data = sem_open(SEM_DATA_NAME, O_CREAT | O_EXCL, 0600, 0);
    if (sem_data == SEM_FAILED) { perror("sem_open(data)"); goto out; }
    sem_result = sem_open(SEM_RESULT_NAME, O_CREAT | O_EXCL, 0600, 0);
    if (sem_result == SEM_FAILED) { perror("sem_open(result)"); goto out; }
    printf("[PARENT pid=%d] created semaphores %s and %s (initial value 0)\n",
           getpid(), SEM_DATA_NAME, SEM_RESULT_NAME);

    fflush(stdout);                          /* avoid duplicated buffered output */
    pid = fork();
    if (pid < 0) { perror("fork"); goto out; }
    if (pid == 0) child_consumer(sh, sem_data, sem_result);   /* never returns */

    /* ----- parent: producer ----- */
    sh->count = n;
    long expected = 0;
    int expected_max = 0;
    for (int i = 0; i < n; i++) {
        sh->job_id[i] = jobs[i].id;
        sh->burst[i]  = jobs[i].burst;
        expected += jobs[i].burst;
        if (jobs[i].burst > expected_max) expected_max = jobs[i].burst;
    }
    printf("[PARENT pid=%d] wrote %d job burst times into shared memory\n", getpid(), n);
    printf("[PARENT pid=%d] posting semaphore '%s'\n", getpid(), SEM_DATA_NAME);
    fflush(stdout);
    sem_post(sem_data);

    if (sem_wait_retry(sem_result) == -1) { perror("parent sem_wait"); goto out; }
    printf("[PARENT pid=%d] result received: total=%ld avg=%.2f max=%d\n",
           getpid(), sh->total_burst, sh->avg_burst, sh->max_burst);
    printf("[PARENT pid=%d] verification: expected total=%ld max=%d -> %s\n",
           getpid(), expected, expected_max,
           (sh->total_burst == expected && sh->max_burst == expected_max)
               ? "MATCH" : "MISMATCH");

    int status;
    if (waitpid(pid, &status, 0) == -1) { perror("waitpid"); goto out; }
    pid = -1;
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        printf("[PARENT] child exited normally\n");
        rc = (sh->total_burst == expected) ? 0 : -1;
    }

out:
    /* make sure a child never stays behind if we failed midway */
    if (pid > 0) { waitpid(pid, NULL, 0); }
    if (sh != MAP_FAILED)         munmap(sh, sizeof(SharedData));
    if (fd != -1)                 close(fd);
    if (sem_data != SEM_FAILED)   sem_close(sem_data);
    if (sem_result != SEM_FAILED) sem_close(sem_result);
    cleanup_ipc(1);                          /* shm_unlink + sem_unlink */
    return rc;
}
