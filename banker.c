/*
 * banker.c - FR5: Banker's safety algorithm
 *
 * Input file format ('#' starts a comment):
 *     <n_processes> <m_resource_types>
 *     Available   : m numbers
 *     Allocation  : n rows of m numbers
 *     Maximum     : n rows of m numbers
 * Need[i][j] = Maximum[i][j] - Allocation[i][j]
 */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "campuscompute.h"

#define MAXP 10
#define MAXR 10

/* reads the next integer, skipping whitespace and '#' comments */
static int read_int(FILE *fp, int *out)
{
    int c;
    while ((c = fgetc(fp)) != EOF) {
        if (isspace(c)) continue;
        if (c == '#') {
            while ((c = fgetc(fp)) != EOF && c != '\n') { }
            continue;
        }
        ungetc(c, fp);
        break;
    }
    if (c == EOF) return 0;
    return fscanf(fp, "%d", out) == 1;
}

static void print_matrix(const char *title, int rows, int cols, int m[MAXP][MAXR])
{
    printf("\n%s:\n      ", title);
    for (int j = 0; j < cols; j++) printf("R%-3d", j);
    printf("\n");
    for (int i = 0; i < rows; i++) {
        printf("P%-4d ", i);
        for (int j = 0; j < cols; j++) printf("%-4d", m[i][j]);
        printf("\n");
    }
}

/* Returns 0 if the file was processed (result in *is_safe), -1 on error. */
int run_banker(const char *file, int *is_safe)
{
    int n, m;
    int avail[MAXR], alloc[MAXP][MAXR], max[MAXP][MAXR], need[MAXP][MAXR];

    FILE *fp = fopen(file, "r");
    if (!fp) {
        fprintf(stderr, "Error: cannot open Banker's input file '%s': %s\n",
                file, strerror(errno));
        return -1;
    }

    if (!read_int(fp, &n) || !read_int(fp, &m) ||
        n < 1 || n > MAXP || m < 1 || m > MAXR) {
        fprintf(stderr, "Error: '%s': invalid process/resource counts "
                "(1..%d processes, 1..%d resource types)\n", file, MAXP, MAXR);
        fclose(fp);
        return -1;
    }
    for (int j = 0; j < m; j++)
        if (!read_int(fp, &avail[j]) || avail[j] < 0) goto bad;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < m; j++)
            if (!read_int(fp, &alloc[i][j]) || alloc[i][j] < 0) goto bad;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < m; j++)
            if (!read_int(fp, &max[i][j]) || max[i][j] < 0) goto bad;
    fclose(fp);

    for (int i = 0; i < n; i++)
        for (int j = 0; j < m; j++) {
            if (alloc[i][j] > max[i][j]) {
                fprintf(stderr, "Error: '%s': Allocation exceeds Maximum for P%d, R%d\n",
                        file, i, j);
                return -1;
            }
            need[i][j] = max[i][j] - alloc[i][j];        /* Need = Max - Allocation */
        }

    printf("\n===== Banker's Algorithm (safety check) : %s =====\n", file);
    printf("Processes = %d, Resource types = %d\n", n, m);
    printf("\nAvailable:\n      ");
    for (int j = 0; j < m; j++) printf("R%-3d", j);
    printf("\n      ");
    for (int j = 0; j < m; j++) printf("%-4d", avail[j]);
    printf("\n");
    print_matrix("Allocation", n, m, alloc);
    print_matrix("Maximum", n, m, max);
    print_matrix("Need (Maximum - Allocation)", n, m, need);

    /* ---- safety algorithm ---- */
    int work[MAXR], finish[MAXP] = {0}, seq[MAXP], count = 0;
    memcpy(work, avail, sizeof(int) * m);

    printf("\nSafety algorithm steps:\n");
    int progress = 1;
    while (count < n && progress) {
        progress = 0;
        for (int i = 0; i < n; i++) {
            if (finish[i]) continue;
            int ok = 1;
            for (int j = 0; j < m; j++)
                if (need[i][j] > work[j]) { ok = 0; break; }
            if (ok) {
                for (int j = 0; j < m; j++) work[j] += alloc[i][j];
                finish[i] = 1;
                seq[count++] = i;
                progress = 1;
                printf("  P%d can finish (Need <= Work). Work becomes [", i);
                for (int j = 0; j < m; j++) printf("%d%s", work[j], j < m - 1 ? " " : "");
                printf("]\n");
            }
        }
    }

    if (count == n) {
        printf("\nResult: SAFE state. Safe sequence: < ");
        for (int i = 0; i < n; i++) printf("P%d%s", seq[i], i < n - 1 ? " -> " : " ");
        printf(">\n");
        if (is_safe) *is_safe = 1;
    } else {
        printf("\nResult: UNSAFE state. No process order can finish; stuck processes:");
        for (int i = 0; i < n; i++)
            if (!finish[i]) printf(" P%d", i);
        printf("\nGranting these resources may lead to DEADLOCK.\n");
        if (is_safe) *is_safe = 0;
    }
    return 0;

bad:
    fprintf(stderr, "Error: '%s': malformed or negative number in input\n", file);
    fclose(fp);
    return -1;
}
