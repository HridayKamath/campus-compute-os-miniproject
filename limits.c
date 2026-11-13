/*
 * limits.c - FR7: process resource limits with setrlimit()
 *
 * Both experiments run inside a CHILD process so the limits only affect that
 * child and never the main program.
 *   1. RLIMIT_CPU    : soft limit 1 second of CPU time. The child spins in a
 *                      busy loop; the kernel sends SIGXCPU and the child dies.
 *   2. RLIMIT_NOFILE : soft limit 10 open files. The child opens /dev/null
 *                      repeatedly until open() fails with EMFILE.
 */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "campuscompute.h"

static void cpu_child(void)
{
    struct rlimit rl = { .rlim_cur = 1, .rlim_max = 3 };
    struct rlimit nocore = { 0, 0 };
    setrlimit(RLIMIT_CORE, &nocore);              /* no core file from SIGXCPU */

    if (setrlimit(RLIMIT_CPU, &rl) == -1) { perror("setrlimit(RLIMIT_CPU)"); _exit(2); }
    getrlimit(RLIMIT_CPU, &rl);
    printf("[CHILD pid=%d] RLIMIT_CPU set: soft=%lu s, hard=%lu s\n",
           getpid(), (unsigned long)rl.rlim_cur, (unsigned long)rl.rlim_max);
    printf("[CHILD pid=%d] starting an endless busy loop ...\n", getpid());
    fflush(stdout);

    volatile unsigned long x = 0;
    for (;;) x++;                                 /* killed by SIGXCPU */
}

static void nofile_child(void)
{
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == -1) { perror("getrlimit"); _exit(2); }
    rl.rlim_cur = 10;                             /* keep the hard limit as is */
    if (setrlimit(RLIMIT_NOFILE, &rl) == -1) { perror("setrlimit(RLIMIT_NOFILE)"); _exit(2); }
    printf("[CHILD pid=%d] RLIMIT_NOFILE set: soft=10\n", getpid());

    int fds[64], opened = 0;
    for (;;) {
        int fd = open("/dev/null", O_RDONLY);
        if (fd == -1) {
            printf("[CHILD pid=%d] open() #%d failed: %s (errno=%d)\n",
                   getpid(), opened + 1, strerror(errno), errno);
            break;
        }
        fds[opened++] = fd;
        if (opened >= 64) break;
    }
    printf("[CHILD pid=%d] managed to open %d extra files before hitting the limit\n",
           getpid(), opened);
    for (int i = 0; i < opened; i++) close(fds[i]);   /* clean up our descriptors */
    fflush(stdout);
    _exit(errno == EMFILE ? 0 : 1);
}

/* Runs one experiment in a child and returns its wait status (or -1). */
static int run_child(void (*fn)(void))
{
    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return -1; }
    if (pid == 0) { fn(); _exit(0); }
    int status;
    if (waitpid(pid, &status, 0) == -1) { perror("waitpid"); return -1; }
    return status;
}

int run_limits_demo(void)
{
    int rc = 0;
    printf("\n===== Resource limits (setrlimit) =====\n");

    printf("\n--- Experiment 1: CPU time limit (RLIMIT_CPU) ---\n");
    int st = run_child(cpu_child);
    if (st != -1 && WIFSIGNALED(st) && WTERMSIG(st) == SIGXCPU) {
        printf("[PARENT] Observed effect: child was terminated by signal %d (%s)\n"
               "         -> the kernel enforced the 1-second CPU limit.\n",
               WTERMSIG(st), strsignal(WTERMSIG(st)));
    } else {
        printf("[PARENT] Unexpected result: CPU limit was not observed\n");
        rc = -1;
    }

    printf("\n--- Experiment 2: open-file limit (RLIMIT_NOFILE) ---\n");
    st = run_child(nofile_child);
    if (st != -1 && WIFEXITED(st) && WEXITSTATUS(st) == 0) {
        printf("[PARENT] Observed effect: open() failed with EMFILE once the limit "
               "was reached\n         (child exited normally, all its files closed).\n");
    } else {
        printf("[PARENT] Unexpected result: open-file limit was not observed\n");
        rc = -1;
    }
    return rc;
}
