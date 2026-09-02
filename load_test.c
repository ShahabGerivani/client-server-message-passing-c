#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>

typedef struct {
    long requests_sent;
    long requests_ok;
    long requests_failed;
    double total_latency_ms;
    double max_latency_ms;
} worker_stats_t;

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

/* Safety net: if a worker ever gets stuck blocked on a read (e.g. the
 * server hangs), don't let it hang the whole test run forever. */
static void alarm_handler(int sig) { (void)sig; _exit(1); }

/* Reads lines from the child's stdout until one starting with
 * "server: " shows up (that's the client's response line) or EOF. */
static int wait_for_response(FILE *out_fp)
{
    char *line = NULL;
    size_t cap = 0;
    int result = -1;

    for (;;) {
        ssize_t n = getline(&line, &cap, out_fp);
        if (n < 0) break;                        /* EOF / pipe closed */
        /* client.c's "Enter your command: " prompt has no trailing
         * newline, so it often arrives merged with the next flushed
         * line -- e.g. "Enter your command: server: <resp> \n" -- so
         * search for "server:" anywhere in the line, not just at the
         * start. */
        if (strstr(line, "server:") != NULL) { result = 0; break; }
        /* else: "Connected." banner or an error message -- keep reading */
    }
    free(line);
    return result;
}

/* Forks + execs the REAL client binary, feeds it commands one at a
 * time on its stdin, and times each round trip by watching stdout. */
static void run_client_worker(const char *client_path, int num_requests, worker_stats_t *stats)
{
    int in_pipe[2];   /* parent -> child's stdin  */
    int out_pipe[2];  /* child's stdout -> parent */

    if (pipe(in_pipe) == -1 || pipe(out_pipe) == -1) {
        perror("pipe");
        stats->requests_failed = num_requests;
        return;
    }

    pid_t child = fork();
    if (child == 0) {
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        close(in_pipe[0]); close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);

        /* -oL forces line-buffered stdout so "server: ..." lines show
         * up immediately instead of sitting in a full stdio buffer. */
        execlp("stdbuf", "stdbuf", "-oL", client_path, (char *)NULL);
        execl(client_path, client_path, (char *)NULL); /* fallback: no stdbuf on this system */
        perror("exec client");
        _exit(127);
    } else if (child < 0) {
        perror("fork (client)");
        stats->requests_failed = num_requests;
        return;
    }

    close(in_pipe[0]);
    close(out_pipe[1]);

    FILE *in_fp  = fdopen(in_pipe[1], "w");
    FILE *out_fp = fdopen(out_pipe[0], "r");
    if (!in_fp || !out_fp) {
        perror("fdopen");
        kill(child, SIGKILL);
        waitpid(child, NULL, 0);
        stats->requests_failed = num_requests;
        return;
    }

    const char *paths[] = {".", "/tmp", "/etc"};
    const char *files[] = {"/etc/hostname", "/etc/hosts"};
    char cmdbuf[160];

    for (int i = 0; i < num_requests; i++) {
        const char *line;
        switch (i % 3) {
            case 0: line = "pwd\n"; break;
            case 1: snprintf(cmdbuf, sizeof cmdbuf, "ls %s\n", paths[i % 3]); line = cmdbuf; break;
            default: snprintf(cmdbuf, sizeof cmdbuf, "cat %s\n", files[i % 2]); line = cmdbuf; break;
        }

        double t0 = now_ms();
        if (fputs(line, in_fp) == EOF || fflush(in_fp) == EOF) {
            stats->requests_failed++;
            break; /* child's stdin is gone, no point continuing */
        }

        int ok = (wait_for_response(out_fp) == 0);
        double dt = now_ms() - t0;

        stats->requests_sent++;
        if (ok) {
            stats->requests_ok++;
            stats->total_latency_ms += dt;
            if (dt > stats->max_latency_ms) stats->max_latency_ms = dt;
        } else {
            stats->requests_failed++;
            break; /* child's stdout pipe is likely broken too */
        }
    }

    /* client.c spins forever on stdin EOF instead of exiting, so we
     * close its stdin and kill it rather than waiting it out. */
    fclose(in_fp);
    kill(child, SIGKILL);
    waitpid(child, NULL, 0);
    fclose(out_fp);
}

int main(int argc, char *argv[])
{
    int num_clients = argc > 1 ? atoi(argv[1]) : 10;
    int requests_per_client = argc > 2 ? atoi(argv[2]) : 50;
    const char *client_path = argc > 3 ? argv[3] : "./client";

    if (num_clients <= 0 || requests_per_client <= 0) {
        fprintf(stderr, "Usage: %s [num_clients] [requests_per_client] [path_to_client_binary]\n", argv[0]);
        exit(1);
    }
    if (access(client_path, X_OK) != 0) {
        fprintf(stderr, "Can't execute client binary at '%s': %s\n", client_path, strerror(errno));
        exit(1);
    }

    worker_stats_t *stats = mmap(NULL, sizeof(worker_stats_t) * num_clients,
                                  PROT_READ | PROT_WRITE,
                                  MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (stats == MAP_FAILED) { perror("mmap"); exit(1); }
    memset(stats, 0, sizeof(worker_stats_t) * num_clients);

    printf("Starting load test: %d clients (each execing '%s') x %d requests...\n",
           num_clients, client_path, requests_per_client);

    double start = now_ms();

    for (int i = 0; i < num_clients; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            signal(SIGALRM, alarm_handler);
            alarm(requests_per_client + 30); /* hard cap so a hung worker can't block the run */
            run_client_worker(client_path, requests_per_client, &stats[i]);
            _exit(0);
        } else if (pid < 0) {
            perror("fork (worker)");
        }
    }

    for (int i = 0; i < num_clients; i++) wait(NULL);

    double elapsed = (now_ms() - start) / 1000.0;

    long total_sent = 0, total_ok = 0, total_failed = 0;
    double total_latency = 0, max_latency = 0;
    for (int i = 0; i < num_clients; i++) {
        total_sent    += stats[i].requests_sent;
        total_ok      += stats[i].requests_ok;
        total_failed  += stats[i].requests_failed;
        total_latency += stats[i].total_latency_ms;
        if (stats[i].max_latency_ms > max_latency) max_latency = stats[i].max_latency_ms;
    }

    printf("\n=== Load test results ===\n");
    printf("Clients:            %d\n", num_clients);
    printf("Requests/client:    %d\n", requests_per_client);
    printf("Total requests:     %ld\n", total_sent);
    printf("Successful:         %ld\n", total_ok);
    printf("Failed:             %ld\n", total_failed);
    printf("Elapsed time:       %.2f s\n", elapsed);
    if (elapsed > 0) printf("Throughput:         %.1f req/s\n", total_ok / elapsed);
    if (total_ok > 0) printf("Avg latency:        %.2f ms\n", total_latency / total_ok);
    printf("Max latency:        %.2f ms\n", max_latency);

    munmap(stats, sizeof(worker_stats_t) * num_clients);
    return 0;
}