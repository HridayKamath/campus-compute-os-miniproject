/*
 * campuscompute.h - shared declarations for the CampusCompute project
 * Team: Hriday Kamath (PES1UG25CS207), Gnyanesh J N (PES1UG25CS189)
 */
#ifndef CAMPUSCOMPUTE_H
#define CAMPUSCOMPUTE_H

#include <stdio.h>

#define MAX_JOBS         20
#define MAX_SEGS         2200          /* max Gantt chart segments           */
#define DEFAULT_QUANTUM  3             /* Round Robin time quantum (units)   */
#define DEFAULT_JOBFILE  "jobs.txt"

/* Names of files / IPC objects created by this program (and ONLY these
 * are removed by the cleanup code). */
#define LOG_FILE         "campuscompute.log"
#define SHM_NAME         "/campuscompute_shm"
#define SEM_DATA_NAME    "/campuscompute_sem_data"
#define SEM_RESULT_NAME  "/campuscompute_sem_result"

/* ---------- FR1: job management ---------- */
typedef enum { JOB_NEW, JOB_READY, JOB_RUNNING, JOB_DONE } JobStatus;

typedef struct {
    int id;
    int arrival;
    int burst;
    int priority;       /* smaller number = higher priority */
    int remaining;      /* CPU time still needed            */
    int start;          /* first time the job got the CPU   */
    int completion;
    int turnaround;
    int waiting;
    JobStatus status;
} Job;

typedef enum { ALGO_FCFS, ALGO_RR, ALGO_PRIORITY } Algo;

/* jobs.c */
int         load_jobs(const char *path, Job jobs[], int max_jobs);
void        print_job_table(const Job jobs[], int n);
const char *status_name(JobStatus s);

/* scheduler.c (FR2) */
int run_scheduler(Algo algo, const Job jobs[], int n, int quantum, int verbose,
                  double *avg_wt, double *avg_tat, int *makespan);
int run_compare(const Job jobs[], int n, int quantum);

/* logger.c (FR3) */
int run_logger_demo(void);

/* ipc.c (FR4) */
int run_ipc_demo(const char *jobfile);
int cleanup_ipc(int verbose);

/* banker.c (FR5) */
int run_banker(const char *file, int *is_safe);

/* worker.c (FR6) */
int run_worker_demo(int simulate_failure);

/* limits.c (FR7) */
int run_limits_demo(void);

#endif
