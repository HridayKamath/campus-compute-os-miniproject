/*
 * scheduler.c - FR2: FCFS, Round Robin and non-preemptive Priority scheduling
 *
 * Tie-breaking rules (stated here and in the README):
 *   FCFS     : earlier arrival first; same arrival -> smaller job ID first.
 *   Priority : smaller priority number = higher priority; tie -> earlier
 *              arrival, then smaller job ID.
 *   RR       : ready queue is FIFO. If a new job arrives at the same instant
 *              a running job's quantum expires, the NEW job is queued first,
 *              then the preempted job goes to the back of the queue.
 *   Time unit: abstract "time units"; default RR quantum = 3.
 */
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "campuscompute.h"

typedef struct { int job_id; int start; int end; } Segment;   /* job_id 0 = idle */

static Segment segs[MAX_SEGS];
static int nsegs;

static void add_seg(int job_id, int start, int end)
{
    if (nsegs < MAX_SEGS) {
        segs[nsegs].job_id = job_id;
        segs[nsegs].start  = start;
        segs[nsegs].end    = end;
        nsegs++;
    }
}

/* Fill 'order' with job indexes sorted by arrival time, then job ID. */
static void sort_by_arrival(const Job j[], int n, int order[])
{
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 1; i < n; i++) {                 /* insertion sort */
        int key = order[i], k = i - 1;
        while (k >= 0 &&
               (j[order[k]].arrival > j[key].arrival ||
                (j[order[k]].arrival == j[key].arrival &&
                 j[order[k]].id > j[key].id))) {
            order[k + 1] = order[k];
            k--;
        }
        order[k + 1] = key;
    }
}

/* ------------------------------ FCFS ------------------------------ */
static void sched_fcfs(Job j[], int n)
{
    int order[MAX_JOBS], time = 0;
    sort_by_arrival(j, n, order);

    for (int k = 0; k < n; k++) {
        Job *p = &j[order[k]];
        if (time < p->arrival) {                  /* CPU idle until arrival */
            add_seg(0, time, p->arrival);
            time = p->arrival;
        }
        p->status = JOB_RUNNING;
        p->start  = time;
        add_seg(p->id, time, time + p->burst);
        time += p->burst;
        p->remaining  = 0;
        p->completion = time;
        p->status     = JOB_DONE;
    }
}

/* -------------------- Non-preemptive Priority --------------------- */
static int better_priority(const Job *a, const Job *b)
{
    if (a->priority != b->priority) return a->priority < b->priority;
    if (a->arrival  != b->arrival)  return a->arrival  < b->arrival;
    return a->id < b->id;
}

static void sched_priority(Job j[], int n)
{
    int time = 0, done = 0;

    while (done < n) {
        int best = -1;
        for (int i = 0; i < n; i++)
            if (j[i].status != JOB_DONE && j[i].arrival <= time &&
                (best < 0 || better_priority(&j[i], &j[best])))
                best = i;

        if (best < 0) {                           /* nobody has arrived yet */
            int next = INT_MAX;
            for (int i = 0; i < n; i++)
                if (j[i].status != JOB_DONE && j[i].arrival < next)
                    next = j[i].arrival;
            add_seg(0, time, next);
            time = next;
            continue;
        }

        Job *p = &j[best];
        p->status = JOB_RUNNING;
        p->start  = time;
        add_seg(p->id, time, time + p->burst);    /* runs to completion */
        time += p->burst;
        p->remaining  = 0;
        p->completion = time;
        p->status     = JOB_DONE;
        done++;
    }
}

/* ---------------------------- Round Robin -------------------------- */
/* Circular queue; a job is in the queue at most once, so MAX_JOBS+1 is enough */
static int rq[MAX_JOBS + 1], rq_head, rq_tail;
#define RQ_SIZE (MAX_JOBS + 1)
static void rq_push(int v) { rq[rq_tail] = v; rq_tail = (rq_tail + 1) % RQ_SIZE; }
static int  rq_pop(void)   { int v = rq[rq_head]; rq_head = (rq_head + 1) % RQ_SIZE; return v; }
static int  rq_empty(void) { return rq_head == rq_tail; }

static void sched_rr(Job j[], int n, int quantum)
{
    int order[MAX_JOBS], next_arr = 0, time = 0, done = 0;
    sort_by_arrival(j, n, order);
    rq_head = rq_tail = 0;

    while (done < n) {
        /* admit every job that has arrived by now */
        while (next_arr < n && j[order[next_arr]].arrival <= time) {
            j[order[next_arr]].status = JOB_READY;
            rq_push(order[next_arr++]);
        }
        if (rq_empty()) {                         /* idle gap */
            int t = j[order[next_arr]].arrival;
            add_seg(0, time, t);
            time = t;
            continue;
        }

        int i = rq_pop();
        Job *p = &j[i];
        int slice = p->remaining < quantum ? p->remaining : quantum;
        if (p->start < 0) p->start = time;
        p->status = JOB_RUNNING;
        add_seg(p->id, time, time + slice);
        time += slice;
        p->remaining -= slice;

        /* jobs arriving during the slice are queued BEFORE the preempted job */
        while (next_arr < n && j[order[next_arr]].arrival <= time) {
            j[order[next_arr]].status = JOB_READY;
            rq_push(order[next_arr++]);
        }
        if (p->remaining > 0) {
            p->status = JOB_READY;
            rq_push(i);
        } else {
            p->completion = time;
            p->status = JOB_DONE;
            done++;
        }
    }
}

/* --------------------------- Output helpers ------------------------ */
static void print_gantt(void)
{
    printf("\nExecution order : ");
    for (int i = 0; i < nsegs; i++) {
        if (segs[i].job_id == 0) printf("IDLE");
        else                     printf("J%d", segs[i].job_id);
        if (i < nsegs - 1) printf(" -> ");
    }
    printf("\n\nTimeline (Gantt chart):\n");
    for (int i = 0; i < nsegs; i++) {
        char label[8];
        if (segs[i].job_id == 0) snprintf(label, sizeof label, "IDLE");
        else                     snprintf(label, sizeof label, "J%d", segs[i].job_id);
        printf("| %-4s ", label);
    }
    printf("|\n");
    for (int i = 0; i < nsegs; i++) printf("%-7d", segs[i].start);
    if (nsegs > 0) printf("%d", segs[nsegs - 1].end);
    printf("\n");
}

static const char *algo_name(Algo a)
{
    switch (a) {
    case ALGO_FCFS:     return "FCFS (First Come First Served)";
    case ALGO_RR:       return "Round Robin";
    case ALGO_PRIORITY: return "Non-preemptive Priority";
    }
    return "?";
}

/* Runs one algorithm on a COPY of the jobs so the same input can be reused. */
int run_scheduler(Algo algo, const Job jobs[], int n, int quantum, int verbose,
                  double *avg_wt, double *avg_tat, int *makespan)
{
    Job work[MAX_JOBS];
    if (n <= 0 || n > MAX_JOBS || (algo == ALGO_RR && quantum <= 0)) return -1;

    memcpy(work, jobs, sizeof(Job) * n);
    for (int i = 0; i < n; i++) {
        work[i].remaining = work[i].burst;
        work[i].start     = -1;
        work[i].status    = JOB_NEW;
    }
    nsegs = 0;

    switch (algo) {
    case ALGO_FCFS:     sched_fcfs(work, n);             break;
    case ALGO_RR:       sched_rr(work, n, quantum);      break;
    case ALGO_PRIORITY: sched_priority(work, n);         break;
    }

    double sum_wt = 0, sum_tat = 0;
    int last = 0;
    for (int i = 0; i < n; i++) {
        work[i].turnaround = work[i].completion - work[i].arrival;
        work[i].waiting    = work[i].turnaround - work[i].burst;
        sum_wt  += work[i].waiting;
        sum_tat += work[i].turnaround;
        if (work[i].completion > last) last = work[i].completion;
    }
    if (avg_wt)   *avg_wt   = sum_wt / n;
    if (avg_tat)  *avg_tat  = sum_tat / n;
    if (makespan) *makespan = last;

    if (verbose) {
        printf("\n===== %s =====\n", algo_name(algo));
        if (algo == ALGO_RR)
            printf("Time quantum    : %d time units\n", quantum);
        if (algo == ALGO_FCFS)
            printf("Tie-break rule  : same arrival time -> smaller Job ID first\n");
        if (algo == ALGO_PRIORITY)
            printf("Tie-break rule  : lower priority number runs first; tie -> "
                   "earlier arrival, then smaller Job ID\n");
        if (algo == ALGO_RR)
            printf("Tie-break rule  : FIFO queue; a job arriving when a quantum "
                   "expires is queued before the preempted job\n");
        print_gantt();

        printf("\n%-6s %-8s %-6s %-9s %-11s %-11s %-8s %-6s\n", "JobID", "Arrival",
               "Burst", "Priority", "Completion", "Turnaround", "Waiting", "Status");
        printf("--------------------------------------------------------------------------\n");
        for (int i = 0; i < n; i++)
            printf("J%-5d %-8d %-6d %-9d %-11d %-11d %-8d %-6s\n", work[i].id,
                   work[i].arrival, work[i].burst, work[i].priority,
                   work[i].completion, work[i].turnaround, work[i].waiting,
                   status_name(work[i].status));
        printf("\nAverage Turnaround Time = %.2f\n", *avg_tat);
        printf("Average Waiting Time    = %.2f\n", *avg_wt);
    }
    return 0;
}

/* Runs all three algorithms on the same job set and prints a comparison. */
int run_compare(const Job jobs[], int n, int quantum)
{
    double wt[3], tat[3];
    int span[3];
    Algo algos[3] = { ALGO_FCFS, ALGO_RR, ALGO_PRIORITY };
    const char *names[3] = { "FCFS", "Round Robin", "Priority (non-preempt)" };

    printf("\n===== Scheduling comparison (same job set, RR quantum = %d) =====\n",
           quantum);
    print_job_table(jobs, n);

    for (int a = 0; a < 3; a++)
        if (run_scheduler(algos[a], jobs, n, quantum, 0, &wt[a], &tat[a], &span[a]) != 0)
            return -1;

    printf("\n%-24s %-18s %-18s %-10s\n", "Algorithm", "Avg Turnaround",
           "Avg Waiting", "Makespan");
    printf("--------------------------------------------------------------------\n");
    int best = 0;
    for (int a = 0; a < 3; a++) {
        printf("%-24s %-18.2f %-18.2f %-10d\n", names[a], tat[a], wt[a], span[a]);
        if (wt[a] < wt[best]) best = a;
    }
    printf("\nLowest average waiting time: %s (%.2f)\n", names[best], wt[best]);
    return 0;
}
