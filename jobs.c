/*
 * jobs.c - FR1: job representation, input validation, job table printing
 *
 * Job file format (one job per line, '#' starts a comment):
 *     <job_id> <arrival_time> <cpu_burst> <priority>
 */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "campuscompute.h"

const char *status_name(JobStatus s)
{
    switch (s) {
    case JOB_NEW:     return "NEW";
    case JOB_READY:   return "READY";
    case JOB_RUNNING: return "RUNNING";
    case JOB_DONE:    return "DONE";
    }
    return "?";
}

/* Reads and validates jobs from a file.
 * Returns the number of jobs, or -1 after printing an error message. */
int load_jobs(const char *path, Job jobs[], int max_jobs)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Error: cannot open job file '%s': %s\n",
                path, strerror(errno));
        return -1;
    }

    char line[256];
    int lineno = 0, n = 0;

    while (fgets(line, sizeof line, fp)) {
        lineno++;
        char *p = line;
        while (isspace((unsigned char)*p)) p++;
        if (*p == '#' || *p == '\0') continue;          /* comment / blank */

        int id, arrival, burst, prio;
        char extra;
        /* " %c" makes sure there is no extra garbage after 4 numbers */
        if (sscanf(p, "%d %d %d %d %c", &id, &arrival, &burst, &prio, &extra) != 4) {
            fprintf(stderr, "Error: %s line %d: malformed job (expected: "
                    "<id> <arrival> <burst> <priority>)\n", path, lineno);
            fclose(fp);
            return -1;
        }
        if (id <= 0 || arrival < 0 || arrival > 1000 ||
            burst <= 0 || burst > 100 || prio < 1 || prio > 10) {
            fprintf(stderr, "Error: %s line %d: value out of range "
                    "(id>0, 0<=arrival<=1000, 1<=burst<=100, 1<=priority<=10)\n",
                    path, lineno);
            fclose(fp);
            return -1;
        }
        for (int i = 0; i < n; i++) {
            if (jobs[i].id == id) {
                fprintf(stderr, "Error: %s line %d: duplicate job id %d\n",
                        path, lineno, id);
                fclose(fp);
                return -1;
            }
        }
        if (n >= max_jobs) {
            fprintf(stderr, "Error: too many jobs (maximum is %d)\n", max_jobs);
            fclose(fp);
            return -1;
        }

        jobs[n].id         = id;
        jobs[n].arrival    = arrival;
        jobs[n].burst      = burst;
        jobs[n].priority   = prio;
        jobs[n].remaining  = burst;
        jobs[n].start      = -1;
        jobs[n].completion = 0;
        jobs[n].turnaround = 0;
        jobs[n].waiting    = 0;
        jobs[n].status     = JOB_NEW;
        n++;
    }
    fclose(fp);

    if (n == 0) {
        fprintf(stderr, "Error: job file '%s' contains no jobs\n", path);
        return -1;
    }
    return n;
}

void print_job_table(const Job jobs[], int n)
{
    printf("\n%-6s %-8s %-6s %-9s %-8s\n",
           "JobID", "Arrival", "Burst", "Priority", "Status");
    printf("--------------------------------------------\n");
    for (int i = 0; i < n; i++)
        printf("J%-5d %-8d %-6d %-9d %-8s\n", jobs[i].id, jobs[i].arrival,
               jobs[i].burst, jobs[i].priority, status_name(jobs[i].status));
}
