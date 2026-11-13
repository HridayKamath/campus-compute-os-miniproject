# CampusCompute: Intelligent Job Scheduling and Resource Management

Operating Systems (UE25CS242A) - Level 3 Mini-Project, PES University

## Team Members

| Name           | SRN            |
|----------------|----------------|
| Hriday Kamath  | PES1UG25CS207  |
| Gnyanesh J N   | PES1UG25CS189  |

## Contents

1. [Overview](#1-overview)
2. [Quick Start](#2-quick-start)
3. [Features and Requirement Mapping](#3-features-and-requirement-mapping)
4. [Project Structure](#4-project-structure)
5. [Prerequisites and Build](#5-prerequisites-and-build)
6. [Commands](#6-commands)
7. [Input File Formats](#7-input-file-formats)
8. [Design Notes](#8-design-notes)
9. [Sample Output](#9-sample-output)
10. [Cleanup](#10-cleanup)
11. [Testing](#11-testing)
12. [Troubleshooting](#12-troubleshooting)
13. [Assumptions and Limitations](#13-assumptions-and-limitations)

## 1. Overview

A university runs a shared computing lab where students and researchers submit
CPU-intensive jobs. **CampusCompute** is a command-line prototype written in C
for Linux (Ubuntu / WSL Ubuntu). It demonstrates the operating-system concepts
an administrator of such a lab would need:

* job management and CPU scheduling
* a synchronized, multi-threaded activity logger
* inter-process communication
* deadlock avoidance (resource-safety checking)
* worker-process failure detection
* process resource limits and cleanup

Every result is computed by real code at run time. Nothing is hard-coded.

## 2. Quick Start

```bash
cd CampusCompute
make clean && make          # build
./campuscompute help        # list commands
make test                   # run every feature, prints a PASS/FAIL summary
```

## 3. Features and Requirement Mapping

| Req | Feature | OS concepts | File | Command |
|-----|---------|-------------|------|---------|
| FR1 | Job structure (ID, arrival, burst, priority, status), input validation, status tracking | process lifecycle (NEW, READY, RUNNING, DONE) | `jobs.c` | `jobs [file]` |
| FR2 | FCFS, Round Robin, non-preemptive Priority; Gantt timeline; completion, turnaround and waiting times; averages; comparison | CPU scheduling | `scheduler.c` | `sched ...` |
| FR3 | Bounded producer-consumer logger writing to a log file | pthreads, mutex, condition variables | `logger.c` | `logger` |
| FR4 | Parent and child exchange data through shared memory, synchronized by two named semaphores | `shm_open`, `mmap`, `sem_open` | `ipc.c` | `ipc [file]` |
| FR5 | Banker's safety algorithm with one safe and one unsafe test case | deadlock avoidance | `banker.c` | `banker safe` / `banker unsafe` |
| FR6 | Worker processes with normal completion and **simulated** failures | `fork`, `waitpid`, exit status and signals | `worker.c` | `worker normal` / `worker fail` |
| FR7 | CPU-time and open-file limits; cleanup of threads, processes, files, shared memory and semaphores | `setrlimit`, resource cleanup | `limits.c`, `ipc.c` | `limits`, `cleanup` |
| FR8 | Command-line interface with usage message and error handling for invalid commands and input | CLI design | `main.c` | `help`, any invalid command |

An extra command, `demo`, runs every feature once and prints a summary.

## 4. Project Structure

```
CampusCompute/
├── campuscompute.h     shared declarations and constants
├── main.c              command-line interface (FR8) and integrated demo
├── jobs.c              job loading and validation (FR1)
├── scheduler.c         FCFS, RR, Priority, comparison (FR2)
├── logger.c            producer-consumer logger (FR3)
├── ipc.c               shared memory, semaphores, IPC cleanup (FR4, FR7)
├── banker.c            Banker's algorithm (FR5)
├── worker.c            fork/waitpid worker demo (FR6)
├── limits.c            setrlimit demo (FR7)
├── Makefile
├── jobs.txt            sample job set (valid)
├── bad_jobs.txt        deliberately invalid job file, for error testing
├── banker_safe.txt     Banker's input: safe state
├── banker_unsafe.txt   Banker's input: unsafe state
└── README.md
```

## 5. Prerequisites and Build

Requirements: Ubuntu (VM) or WSL Ubuntu, `gcc` and `make`.

```bash
sudo apt update
sudo apt install -y build-essential
gcc --version
make --version
```

Build:

```bash
make clean
make
```

This produces the executable `./campuscompute`. Run it from the project
directory, because it reads the sample files (`jobs.txt`, `banker_safe.txt`,
`banker_unsafe.txt`) from the current directory.

Make targets:

| Target | Purpose |
|--------|---------|
| `make` | build the executable |
| `make test` | run the integrated demo (`./campuscompute demo`) |
| `make cleanipc` | remove program-created shared memory and semaphores |
| `make clean` | remove object files, the executable and the log file |

## 6. Commands

```bash
./campuscompute help                        # list of commands
./campuscompute jobs                        # load and validate jobs.txt
./campuscompute sched fcfs                  # FCFS
./campuscompute sched rr jobs.txt 3         # Round Robin, quantum 3
./campuscompute sched priority              # non-preemptive priority
./campuscompute sched compare               # all three on the same jobs
./campuscompute logger                      # producer-consumer logger
./campuscompute ipc                         # shared memory + semaphores
./campuscompute banker safe                 # Banker's: safe state
./campuscompute banker unsafe               # Banker's: unsafe state
./campuscompute worker normal               # workers finish normally
./campuscompute worker fail                 # SIMULATED worker failures
./campuscompute limits                      # setrlimit demo
./campuscompute cleanup                     # remove IPC objects and verify
./campuscompute demo                        # run everything
```

Optional arguments: `sched` accepts a job file and (for `rr` and `compare`) a
time quantum, for example `./campuscompute sched rr jobs.txt 5`. `banker`
accepts `safe`, `unsafe` or the path of your own input file.

Invalid input prints an error message followed by the usage text, and the
program exits with status 1:

```bash
./campuscompute foo                         # unknown command
./campuscompute sched fcfs bad_jobs.txt     # invalid job file (negative burst)
./campuscompute sched rr jobs.txt abc       # invalid time quantum
./campuscompute worker xyz                  # unknown worker mode
```

## 7. Input File Formats

**Job file** (`jobs.txt`): one job per line, `#` starts a comment.

```
<job_id> <arrival_time> <cpu_burst> <priority>
```

Validation rules: id > 0 and unique; 0 <= arrival <= 1000; 1 <= burst <= 100;
1 <= priority <= 10 (1 is the highest priority); at most 20 jobs; exactly four
numbers per line.

**Banker's file**: `n m` on the first line, then Available (m numbers),
Allocation (n rows of m numbers) and Maximum (n rows of m numbers).
`Need = Maximum - Allocation` is computed by the program. Allocation may never
exceed Maximum. `#` starts a comment.

## 8. Design Notes

### Scheduling (FR2)

* **Time unit:** abstract time units. The CPU stays idle if no job has arrived yet.
* **FCFS tie-break:** jobs with the same arrival time run in order of smaller job ID.
* **Priority:** non-preemptive; a smaller priority number means higher priority.
  Tie-break: earlier arrival, then smaller job ID.
* **Round Robin:** default time quantum is **3** (change it with
  `sched rr jobs.txt <quantum>`). The ready queue is FIFO. If a job arrives at
  the moment another job's quantum expires, the new job is queued before the
  preempted job.
* Turnaround time = Completion - Arrival. Waiting time = Turnaround - Burst.
* Each algorithm runs on a copy of the job list, so `sched compare` uses exactly
  the same input for all three.

### Logger (FR3)

Three producer threads put events into a circular queue of size 5, and one
consumer thread writes them to `campuscompute.log`.

* A **mutex** protects the queue.
* The condition variable `not_full` makes producers wait when the queue is full.
* The condition variable `not_empty` makes the consumer wait when it is empty.
* The consumer is deliberately slow so the "queue FULL" case actually happens.
* For termination, each producer counts itself as finished. The consumer exits
  only when all producers are finished and the queue is empty. The main thread
  then joins every thread.
* The log file is recreated on every run.

### Inter-process communication (FR4)

1. The parent creates shared memory `/campuscompute_shm` and two semaphores
   (`/campuscompute_sem_data`, `/campuscompute_sem_result`), then forks a child.
2. The parent writes the job burst times into shared memory and posts the data
   semaphore.
3. The child waits on it, computes total, average and maximum burst, writes the
   result back and posts the result semaphore.
4. The parent verifies the result, collects the child with `waitpid()` and
   removes everything it created.

### Banker's algorithm (FR5)

Standard safety algorithm with `Work = Available` and a `Finish` array. For a
safe state the safe sequence is printed. For an unsafe state the processes
that cannot finish are listed.

* `banker_safe.txt`: safe sequence `P1 -> P3 -> P4 -> P0 -> P2`
* `banker_unsafe.txt`: no process can finish, so the state is unsafe

### Worker failure detection (FR6)

The parent forks 3 workers and calls `waitpid()` on each one. In `fail` mode,
worker 2 exits with code 3 and worker 3 sends `SIGTERM` to itself. **These
failures are simulated by the program and are labelled as simulated in the
output. They are not real OS failures.** The parent decodes the status with
`WIFEXITED`, `WEXITSTATUS`, `WIFSIGNALED` and `WTERMSIG`.

### Resource limits (FR7)

Both experiments run in a child process, so the limits never affect the main program.

1. `RLIMIT_CPU` = 1 second: the child busy-loops and the kernel kills it with
   `SIGXCPU` (signal 24). The parent reports this.
2. `RLIMIT_NOFILE` = 10: the child opens `/dev/null` repeatedly until `open()`
   fails with `EMFILE` ("Too many open files"), then closes everything.

## 9. Sample Output

Scheduling comparison (`./campuscompute sched compare`, quantum 3):

```
Algorithm                Avg Turnaround     Avg Waiting        Makespan
--------------------------------------------------------------------
FCFS                     16.00              10.83              31
Round Robin              18.67              13.50              31
Priority (non-preempt)   13.67              8.50               31

Lowest average waiting time: Priority (non-preempt) (8.50)
```

Banker's algorithm (`./campuscompute banker safe`):

```
Result: SAFE state. Safe sequence: < P1 -> P3 -> P4 -> P0 -> P2 >
```

Worker failure detection (`./campuscompute worker fail`):

```
[PARENT] worker 1 (pid=...): NORMAL completion, exit code 0
[PARENT] worker 2 (pid=...): FAILURE detected, exit code 3 (simulated failure)
[PARENT] worker 3 (pid=...): FAILURE detected, killed by signal 15 (Terminated) (simulated failure)
```

Integrated demo (`./campuscompute demo`) ends with:

```
  Result: 9 of 9 checks passed
```

Process IDs and timestamps differ on every run.

## 10. Cleanup

The program removes only the objects it created:

* `/dev/shm/campuscompute_shm`
* `/dev/shm/sem.campuscompute_sem_data`
* `/dev/shm/sem.campuscompute_sem_result`

The `ipc` command removes these automatically when it finishes. After an
interrupted run (for example Ctrl+C), clean up manually:

```bash
./campuscompute cleanup      # or: make cleanipc
```

To remove build output and the generated log file:

```bash
make clean
```

To verify that nothing is left behind:

```bash
ls /dev/shm | grep campuscompute       # prints nothing when clean
ps -ef | grep [c]ampuscompute          # prints nothing when no process is running
```

## 11. Testing

Each feature has its own command (section 6) and can be tested independently.
`make test` (the same as `./campuscompute demo`) runs all of them and ends with
a PASS/FAIL summary. The exit status is 0 when every check passes.

| Test | How to run | Expected result |
|------|------------|-----------------|
| Valid jobs load | `./campuscompute jobs` | 6 jobs listed |
| Invalid job file rejected | `./campuscompute sched fcfs bad_jobs.txt` | error with line number, exit status 1 |
| Unknown command | `./campuscompute foo` | error plus usage text |
| Safe state | `./campuscompute banker safe` | safe sequence printed |
| Unsafe state | `./campuscompute banker unsafe` | UNSAFE reported |
| Normal workers | `./campuscompute worker normal` | 3 normal, 0 failed |
| Simulated failure | `./campuscompute worker fail` | 1 normal, 2 failed (labelled simulated) |
| CPU limit | `./campuscompute limits` | child killed by SIGXCPU |
| IPC cleanup | `./campuscompute ipc` then `ls /dev/shm \| grep campuscompute` | no output |

## 12. Troubleshooting

| Problem | Fix |
|---------|-----|
| `gcc: command not found` or `make: command not found` | `sudo apt install -y build-essential` |
| `Error: cannot open job file 'jobs.txt'` | run the program from inside the `CampusCompute` folder |
| `Permission denied` when running `./campuscompute` | `chmod +x campuscompute`, or rebuild with `make` |
| Leftover `campuscompute` entries in `/dev/shm` after Ctrl+C | `./campuscompute cleanup` (the `ipc` command also removes its own stale objects at startup) |
| Output looks stale after editing the code | `make clean && make` |


