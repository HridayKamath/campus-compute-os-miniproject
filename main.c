/*
 * main.c - FR8: command-line interface for CampusCompute
 *
 * Usage: ./campuscompute <command> [arguments]   (run "help" for the list)
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "campuscompute.h"

static void print_usage(FILE *out)
{
    fprintf(out,
        "CampusCompute - job scheduling and resource management prototype\n"
        "Team: Hriday Kamath (PES1UG25CS207), Gnyanesh J N (PES1UG25CS189)\n"
        "\n"
        "Usage: ./campuscompute <command> [arguments]\n"
        "\n"
        "Commands:\n"
        "  help                              Show this help message\n"
        "  jobs    [jobfile]                 Load and validate a job file (default: jobs.txt)\n"
        "  sched fcfs     [jobfile]          FCFS scheduling\n"
        "  sched rr       [jobfile] [quantum] Round Robin (default quantum = 3)\n"
        "  sched priority [jobfile]          Non-preemptive priority scheduling\n"
        "  sched compare  [jobfile] [quantum] Compare all three on the same jobs\n"
        "  logger                            Producer-consumer logger (pthreads), writes campuscompute.log\n"
        "  ipc     [jobfile]                 Shared memory + named semaphore demo\n"
        "  banker  safe|unsafe|<file>        Banker's safety algorithm\n"
        "  worker  normal|fail               fork/waitpid worker demo (fail = SIMULATED failure)\n"
        "  limits                            setrlimit demo (CPU time and open files)\n"
        "  cleanup                           Remove program-created shared memory/semaphores and verify\n"
        "  demo                              Run every feature and print a summary\n");
}

static int usage_error(const char *fmt, const char *arg)
{
    fprintf(stderr, "Error: ");
    fprintf(stderr, fmt, arg);
    fprintf(stderr, "\n\n");
    print_usage(stderr);
    return 1;
}

/* Parses a positive integer; returns -1 if it is not one. */
static int parse_positive(const char *s)
{
    char *end;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno || end == s || *end != '\0' || v <= 0 || v > 1000) return -1;
    return (int)v;
}

static int cmd_sched(int argc, char **argv)
{
    if (argc < 3) return usage_error("'sched' needs an algorithm: %s", "fcfs|rr|priority|compare");
    const char *algo = argv[2];
    const char *file = (argc > 3) ? argv[3] : DEFAULT_JOBFILE;
    int quantum = DEFAULT_QUANTUM;
    int is_fcfs = !strcmp(algo, "fcfs"), is_rr = !strcmp(algo, "rr");
    int is_prio = !strcmp(algo, "priority"), is_cmp = !strcmp(algo, "compare");

    if (!is_fcfs && !is_rr && !is_prio && !is_cmp)
        return usage_error("unknown scheduling algorithm '%s'", algo);

    int max_args = (is_rr || is_cmp) ? 5 : 4;
    if (argc > max_args) return usage_error("too many arguments for 'sched %s'", algo);
    if (argc > 4) {
        quantum = parse_positive(argv[4]);
        if (quantum < 0)
            return usage_error("invalid time quantum '%s' (must be a positive integer)", argv[4]);
    }

    Job jobs[MAX_JOBS];
    int n = load_jobs(file, jobs, MAX_JOBS);
    if (n < 0) return 1;

    double wt, tat;
    int span;
    if (is_cmp) return run_compare(jobs, n, quantum) == 0 ? 0 : 1;

    Algo a = is_fcfs ? ALGO_FCFS : is_rr ? ALGO_RR : ALGO_PRIORITY;
    return run_scheduler(a, jobs, n, quantum, 1, &wt, &tat, &span) == 0 ? 0 : 1;
}

static const char *banker_file(const char *arg)
{
    if (!strcmp(arg, "safe"))   return "banker_safe.txt";
    if (!strcmp(arg, "unsafe")) return "banker_unsafe.txt";
    return arg;
}

/* Runs every feature once and prints a PASS/FAIL summary (S21). */
static int cmd_demo(void)
{
    Job jobs[MAX_JOBS];
    int n = load_jobs(DEFAULT_JOBFILE, jobs, MAX_JOBS);
    if (n < 0) return 1;

    const char *names[10];
    int ok[10], k = 0, safe = -1;

    printf("############ CampusCompute integrated demonstration ############\n");

    printf("\n>>>>> [1/9] FCFS scheduling\n");
    double w, t; int s;
    names[k] = "FCFS scheduling";
    ok[k++] = (run_scheduler(ALGO_FCFS, jobs, n, DEFAULT_QUANTUM, 1, &w, &t, &s) == 0);

    printf("\n>>>>> [2/9] Round Robin and Priority scheduling + comparison\n");
    names[k] = "Round Robin / Priority / comparison";
    int r1 = run_scheduler(ALGO_RR, jobs, n, DEFAULT_QUANTUM, 1, &w, &t, &s);
    int r2 = run_scheduler(ALGO_PRIORITY, jobs, n, DEFAULT_QUANTUM, 1, &w, &t, &s);
    int r3 = run_compare(jobs, n, DEFAULT_QUANTUM);
    ok[k++] = (r1 == 0 && r2 == 0 && r3 == 0);

    printf("\n>>>>> [3/9] Concurrent logger\n");
    names[k] = "Producer-consumer logger (threads)";
    ok[k++] = (run_logger_demo() == 0);

    printf("\n>>>>> [4/9] Shared-memory IPC\n");
    names[k] = "Shared memory + semaphores (IPC)";
    ok[k++] = (run_ipc_demo(DEFAULT_JOBFILE) == 0);

    printf("\n>>>>> [5/9] Banker's algorithm - safe state\n");
    names[k] = "Banker's: safe state detected";
    ok[k++] = (run_banker("banker_safe.txt", &safe) == 0 && safe == 1);

    printf("\n>>>>> [6/9] Banker's algorithm - unsafe state\n");
    names[k] = "Banker's: unsafe state detected";
    ok[k++] = (run_banker("banker_unsafe.txt", &safe) == 0 && safe == 0);

    printf("\n>>>>> [7/9] Worker processes (normal and simulated failure)\n");
    names[k] = "Workers: normal + simulated failure";
    int w1 = run_worker_demo(0);
    int w2 = run_worker_demo(1);
    ok[k++] = (w1 == 0 && w2 == 0);

    printf("\n>>>>> [8/9] Resource limits\n");
    names[k] = "setrlimit: CPU time + open files";
    ok[k++] = (run_limits_demo() == 0);

    printf("\n>>>>> [9/9] Cleanup\n");
    names[k] = "IPC cleanup";
    ok[k++] = (cleanup_ipc(1) == 0);

    int pass = 0;
    printf("\n################ DEMO SUMMARY ################\n");
    for (int i = 0; i < k; i++) {
        printf("  %-40s : %s\n", names[i], ok[i] ? "PASS" : "FAIL");
        pass += ok[i];
    }
    printf("  Result: %d of %d checks passed\n", pass, k);
    printf("  Log file written: %s\n", LOG_FILE);
    return pass == k ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        print_usage(stdout);
        return 0;
    }
    const char *cmd = argv[1];

    if (!strcmp(cmd, "help") || !strcmp(cmd, "-h") || !strcmp(cmd, "--help")) {
        print_usage(stdout);
        return 0;
    }

    if (!strcmp(cmd, "jobs")) {
        if (argc > 3) return usage_error("too many arguments for '%s'", cmd);
        Job jobs[MAX_JOBS];
        const char *file = (argc > 2) ? argv[2] : DEFAULT_JOBFILE;
        int n = load_jobs(file, jobs, MAX_JOBS);
        if (n < 0) return 1;
        printf("Loaded %d valid jobs from '%s'\n", n, file);
        print_job_table(jobs, n);
        return 0;
    }

    if (!strcmp(cmd, "sched")) return cmd_sched(argc, argv);

    if (!strcmp(cmd, "logger")) {
        if (argc > 2) return usage_error("too many arguments for '%s'", cmd);
        return run_logger_demo() == 0 ? 0 : 1;
    }

    if (!strcmp(cmd, "ipc")) {
        if (argc > 3) return usage_error("too many arguments for '%s'", cmd);
        return run_ipc_demo(argc > 2 ? argv[2] : DEFAULT_JOBFILE) == 0 ? 0 : 1;
    }

    if (!strcmp(cmd, "banker")) {
        if (argc != 3) return usage_error("'banker' needs one argument: %s", "safe|unsafe|<file>");
        int safe;
        return run_banker(banker_file(argv[2]), &safe) == 0 ? 0 : 1;
    }

    if (!strcmp(cmd, "worker")) {
        if (argc != 3) return usage_error("'worker' needs one argument: %s", "normal|fail");
        if (!strcmp(argv[2], "normal")) return run_worker_demo(0) == 0 ? 0 : 1;
        if (!strcmp(argv[2], "fail"))   return run_worker_demo(1) == 0 ? 0 : 1;
        return usage_error("unknown worker mode '%s' (use normal or fail)", argv[2]);
    }

    if (!strcmp(cmd, "limits")) {
        if (argc > 2) return usage_error("too many arguments for '%s'", cmd);
        return run_limits_demo() == 0 ? 0 : 1;
    }

    if (!strcmp(cmd, "cleanup")) {
        if (argc > 2) return usage_error("too many arguments for '%s'", cmd);
        return cleanup_ipc(1) == 0 ? 0 : 1;
    }

    if (!strcmp(cmd, "demo")) {
        if (argc > 2) return usage_error("too many arguments for '%s'", cmd);
        return cmd_demo();
    }

    return usage_error("unknown command '%s'", cmd);
}
