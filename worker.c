/*
 * worker.c - FR6: worker failure detection with fork() and waitpid()
 *
 * The parent creates 3 worker processes.
 *   normal mode : every worker exits with status 0.
 *   fail mode   : worker 2 exits with code 3 and worker 3 kills itself with
 *                 SIGTERM. BOTH FAILURES ARE SIMULATED by the program - they
 *                 are not real operating-system faults.
 * The parent calls waitpid() for each child and decodes the status with
 * WIFEXITED / WEXITSTATUS / WIFSIGNALED / WTERMSIG.
 */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "campuscompute.h"

#define NUM_WORKERS 3

/* Code run by each child; never returns. */
static void worker_main(int idx, int simulate_failure)
{
    printf("[WORKER %d pid=%d] started\n", idx, getpid());
    fflush(stdout);
    usleep(200000 * idx);                         /* pretend to do some work */

    if (simulate_failure && idx == 2) {
        printf("[WORKER %d pid=%d] SIMULATED FAILURE: exiting with code 3\n",
               idx, getpid());
        fflush(stdout);
        _exit(3);
    }
    if (simulate_failure && idx == 3) {
        printf("[WORKER %d pid=%d] SIMULATED FAILURE: sending SIGTERM to itself\n",
               idx, getpid());
        fflush(stdout);
        kill(getpid(), SIGTERM);
        pause();
    }
    printf("[WORKER %d pid=%d] finished work normally\n", idx, getpid());
    fflush(stdout);
    _exit(0);
}

/* Returns 0 if the observed results match what the mode should produce. */
int run_worker_demo(int simulate_failure)
{
    pid_t pids[NUM_WORKERS + 1];
    int normal = 0, failed = 0, created = 0;

    printf("\n===== Worker failure detection (%s mode) =====\n",
           simulate_failure ? "SIMULATED-FAILURE" : "normal");
    if (simulate_failure)
        printf("NOTE: worker failures below are SIMULATED by this program, "
               "not real OS failures.\n");
    printf("\n");
    fflush(stdout);                               /* avoid duplicate output after fork */

    for (int i = 1; i <= NUM_WORKERS; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            break;
        }
        if (pid == 0) worker_main(i, simulate_failure);
        pids[i] = pid;
        created++;
        printf("[PARENT] created worker %d (pid=%d)\n", i, pid);
        fflush(stdout);       /* children must not inherit unflushed output */
    }
    fflush(stdout);

    for (int i = 1; i <= created; i++) {
        int status;
        pid_t r;
        while ((r = waitpid(pids[i], &status, 0)) == -1 && errno == EINTR) { }
        if (r == -1) {
            perror("waitpid");
            failed++;
            continue;
        }
        if (WIFEXITED(status)) {
            int code = WEXITSTATUS(status);
            if (code == 0) {
                printf("[PARENT] worker %d (pid=%d): NORMAL completion, exit code 0\n",
                       i, r);
                normal++;
            } else {
                printf("[PARENT] worker %d (pid=%d): FAILURE detected, exit code %d%s\n",
                       i, r, code, simulate_failure ? " (simulated failure)" : "");
                failed++;
            }
        } else if (WIFSIGNALED(status)) {
            printf("[PARENT] worker %d (pid=%d): FAILURE detected, killed by signal %d (%s)%s\n",
                   i, r, WTERMSIG(status), strsignal(WTERMSIG(status)),
                   simulate_failure ? " (simulated failure)" : "");
            failed++;
        }
    }

    printf("\nSummary: %d worker(s) created, %d completed normally, %d failed\n",
           created, normal, failed);
    printf("All worker processes have been collected with waitpid() (no zombies).\n");

    if (created != NUM_WORKERS) return -1;
    return simulate_failure ? (failed == 2 && normal == 1 ? 0 : -1)
                            : (failed == 0 ? 0 : -1);
}
