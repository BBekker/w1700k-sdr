/* SPDX-License-Identifier: MIT */
#define _GNU_SOURCE
#include "../../include/uapi/w1700k_sdr.h"
#include "channel.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
static int fd;
static FILE *events;
static const char *event_path;
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
static const struct w1700k_sdr_stream_ring *ring;
static const unsigned char *data;
static atomic_int done, failed;
static atomic_uint_fast64_t stop_at = UINT64_MAX;
static volatile sig_atomic_t interrupted;
static uint64_t first_seq;
static uint64_t ns(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000 + t.tv_nsec;
}
static int running(void)
{
    return !atomic_load(&done) && !interrupted && ns() < atomic_load(&stop_at);
}
static double wall_time(void)
{
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}
/* Keep the router's RAM log bounded. The web server follows inode changes. */
static void trim_log(void)
{
    if (ftell(events) < 16 * 1024 * 1024)
        return;
    char *old = NULL;
    if (asprintf(&old, "%s.previous", event_path) < 0)
        return;
    if (!rename(event_path, old)) {
        FILE *fresh = fopen(event_path, "w");
        if (fresh) {
            fclose(events);
            events = fresh;
        } else
            rename(old, event_path);
    }
    free(old);
}
static uint64_t completed(void) { return __atomic_load_n(&ring->completed, __ATOMIC_ACQUIRE); }
static int block(uint32_t address, void *data_, uint32_t bytes, int read_)
{
    struct w1700k_sdr_block b = {.address = address,
                                 .length = bytes,
                                 .data = (uintptr_t)data_,
                                 .flags = read_ ? W1700K_SDR_BLOCK_READ : 0,
                                 .timeout_ms = 1000};
    return ioctl(fd, W1700K_SDR_BLOCK, &b);
}
static void fatal(const char *what)
{
    perror(what);
    atomic_store(&failed, 1);
    kill(getpid(), SIGTERM);
}
struct worker {
    unsigned core, sequence, collected, inflight;
    uint64_t next, raw_samples, lost, compute_ns;
    atomic_uint_fast64_t packets, blocks, dsp_cycles, submit_ns, collect_ns, write_ns, meta_ns,
        output_ns, polls, cycle_blocks;
    struct ble_channel filter;
    int16_t input[4096];
    unsigned used, reset;
    pthread_mutex_t queue_lock;
    pthread_cond_t changed;
    unsigned head, tail;
    struct {
        int8_t iq[4096];
        unsigned reset;
    } queue[16];
};
static int collect(struct worker *w)
{
    if (!w->inflight)
        return 0;
    uint32_t output[256];
    uint64_t until = ns() + 1000000000;
    for (;;) {
        uint64_t t = ns();
        if (block(0x0090e004 + 1024 * w->core, output + 1, 4, 1))
            return -1;
        w->meta_ns += ns() - t;
        w->polls++;
        if ((output[1] >> 4) == w->sequence)
            break;
        if (!running() || ns() > until) {
            errno = ETIMEDOUT;
            return -1;
        }
        sched_yield();
    }
    w->blocks++;
    if (!(output[1] & 15)) {
        w->collected = w->sequence;
        w->inflight = 0;
        return 0;
    }
    if (block(0x0090e000 + 1024 * w->core, output, 32, 1))
        return -1;
    if (output[0] != 0x424c4550 || (output[1] >> 4) != w->sequence) {
        errno = EPROTO;
        return -1;
    }
    unsigned bytes = output[6];
    if (bytes < 32 || bytes > 1024 || (bytes & 3) || output[2] > 12) {
        errno = EPROTO;
        return -1;
    }
    uint64_t t = ns();
    if (bytes > 32 && block(0x0090e020 + 1024 * w->core, output + 8, bytes - 32, 1))
        return -1;
    w->output_ns += ns() - t;
    w->cycle_blocks++;
    w->dsp_cycles += output[7];
    unsigned at = 8;
    pthread_mutex_lock(&log_lock);
    if (output[3])
        fprintf(events,
                "{\"type\":\"error\",\"message\":\"DSP event buffer "
                "overflow\",\"channel\":%u,\"dropped\":%u}\n",
                37 + w->core, output[3]);
    for (unsigned j = 0; j < output[2]; j++) {
        if (at + 2 > bytes / 4) {
            pthread_mutex_unlock(&log_lock);
            errno = EPROTO;
            return -1;
        }
        unsigned sample = output[at++], len = output[at++];
        if (len < 2 || len > 257 || at + (len + 6) / 4 > bytes / 4) {
            pthread_mutex_unlock(&log_lock);
            errno = EPROTO;
            return -1;
        }
        unsigned char *p = (unsigned char *)(output + at);
        fprintf(events,
                "{\"type\":\"packet\",\"time\":%.6f,\"channel\":%u,\"core\":%u,\"sample\":%u,"
                "\"pdu\":\"",
                wall_time(), 37 + w->core, w->core, sample);
        for (unsigned k = 0; k < len; k++)
            fprintf(events, "%02x", p[k]);
        fprintf(events, "\",\"crc\":\"%02x%02x%02x\",\"crc_valid\":true}\n", p[len], p[len + 1],
                p[len + 2]);
        w->packets++;
        at += (len + 6) / 4;
    }
    fflush(events);
    trim_log();
    pthread_mutex_unlock(&log_lock);
    w->collected = w->sequence;
    w->inflight = 0;
    return 0;
}
/* Queue ownership: producer owns head until publication under queue_lock;
 * consumer retains tail through the synchronous ioctl, then releases it. */
static int enqueue(struct worker *w)
{
    pthread_mutex_lock(&w->queue_lock);
    while (w->head - w->tail == 16 && running())
        pthread_cond_wait(&w->changed, &w->queue_lock);
    if (!running()) {
        pthread_mutex_unlock(&w->queue_lock);
        return -1;
    }
    unsigned slot = w->head & 15;
    pthread_mutex_unlock(&w->queue_lock);
    unsigned peak = 0, shift = 0;
    for (unsigned i = 0; i < 4096; i++) {
        unsigned v = w->input[i] < 0 ? -w->input[i] : w->input[i];
        if (v > peak)
            peak = v;
    }
    while (peak > 127) {
        peak >>= 1;
        shift++;
    }
    for (unsigned i = 0; i < 4096; i++)
        w->queue[slot].iq[i] = (int8_t)(w->input[i] >> shift);
    w->queue[slot].reset = w->reset;
    pthread_mutex_lock(&w->queue_lock);
    w->head++;
    pthread_cond_broadcast(&w->changed);
    pthread_mutex_unlock(&w->queue_lock);
    w->used = 0;
    w->reset = 0;
    return 0;
}
static int submit(struct worker *w, unsigned slot)
{
    uint64_t t = ns();
    if (collect(w))
        return -1;
    w->collect_ns += ns() - t;
    t = ns();
    w->sequence = (w->sequence + 1) & 0x0fffffff;
    if (!w->sequence)
        w->sequence = 1;
    struct w1700k_sdr_block b = {.address = 0x0090b000 + 4096 * w->core,
                                 .length = 4096,
                                 .data = (uintptr_t)w->queue[slot].iq,
                                 .timeout_ms = 1000,
                                 .write_count = 3};
    b.write_address[0] = 0x0090fe98 + 32 * w->core;
    b.write_value[0] = w->queue[slot].reset;
    b.write_address[1] = 0x0090fe9c + 32 * w->core;
    b.write_value[1] = 2048;
    b.write_address[2] = 0x0090fe90 + 32 * w->core;
    b.write_value[2] = w->sequence;
    if (ioctl(fd, W1700K_SDR_BLOCK, &b))
        return -1;
    w->inflight = 1;
    w->write_ns += ns() - t;
    return 0;
}
static void *feed_dsp(void *arg)
{
    struct worker *w = arg;
    cpu_set_t cpus;
    CPU_ZERO(&cpus);
    CPU_SET(w->core, &cpus);
    pthread_setaffinity_np(pthread_self(), sizeof(cpus), &cpus);
    /* A completed transfer must wake ahead of the channelizer, rather than
     * wait for the 4 ms scheduler tick. It blocks during each PCIe transfer. */
    struct sched_param priority = {.sched_priority = 1};
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &priority)) {
        errno = EPERM;
        fatal("DSP feeder scheduling");
        return NULL;
    }
    while (running()) {
        pthread_mutex_lock(&w->queue_lock);
        while (w->head == w->tail && running())
            pthread_cond_wait(&w->changed, &w->queue_lock);
        if (!running()) {
            pthread_mutex_unlock(&w->queue_lock);
            break;
        }
        unsigned slot = w->tail & 15;
        pthread_mutex_unlock(&w->queue_lock);
        uint64_t t = ns();
        int rc = submit(w, slot);
        w->submit_ns += ns() - t;
        if (rc) {
            if (running())
                fatal("DSP submission");
            break;
        }
        pthread_mutex_lock(&w->queue_lock);
        w->tail++;
        pthread_cond_broadcast(&w->changed);
        pthread_mutex_unlock(&w->queue_lock);
    }
    return NULL;
}
static void *consume(void *arg)
{
    struct worker *w = arg;
    cpu_set_t cpus;
    CPU_ZERO(&cpus);
    CPU_SET(w->core, &cpus);
    pthread_setaffinity_np(pthread_self(), sizeof(cpus), &cpus);
    ble_channel_init(&w->filter, 37 + w->core);
    w->reset = 1;
    w->next = first_seq;
    uint64_t report = ns();
    int16_t narrow[208];
    while (running()) {
        uint64_t c = completed();
        if (c == w->next) {
            sched_yield();
            continue;
        }
        if (c - w->next > ring->slices - ring->slots - 1024) {
            uint64_t skip = c - w->next - 8;
            w->lost += skip * 2048;
            w->next += skip;
            w->used = 0;
            w->reset = 1;
            ble_channel_init(&w->filter, 37 + w->core);
        }
        unsigned slot = w->next % ring->slices, len = (ring->ctrl[slot] >> 16) & 0x3fff;
        if (len != 8192) {
            errno = EPROTO;
            fatal("raw DMA length");
            break;
        }
        uint64_t compute_start = ns();
        unsigned count = ble_channel_run(&w->filter, (const uint32_t *)(data + (size_t)slot * 8192),
                                         2048, narrow);
        w->compute_ns += ns() - compute_start;
        if (completed() - w->next > ring->slices - ring->slots) {
            errno = EOVERFLOW;
            fatal("raw input overwritten during read");
            break;
        }
        w->next++;
        w->raw_samples += 2048;
        for (unsigned k = 0; k < count; k++) {
            w->input[2 * w->used] = narrow[2 * k];
            w->input[2 * w->used + 1] = narrow[2 * k + 1];
            if (++w->used == 2048) {
                if (enqueue(w))
                    return NULL;
            }
        }
        if (ns() - report >= 1000000000u) {
            pthread_mutex_lock(&log_lock);
            fprintf(events,
                    "{\"type\":\"status\",\"channel\":%u,\"raw_samples\":%llu,\"lost_samples\":%"
                    "llu,\"backlog_samples\":%llu,\"packets\":%llu,\"compute_s\":%.3f,\"submit_s\":"
                    "%.3f,\"collect_s\":%.3f,\"write_s\":%.3f,\"meta_s\":%.3f,"
                    "\"output_s\":%.3f,\"polls\":%llu,\"blocks\":%llu,\"dsp_us\":%.3f}\n",
                    37 + w->core, (unsigned long long)w->raw_samples, (unsigned long long)w->lost,
                    (unsigned long long)((completed() - w->next) * 2048),
                    (unsigned long long)w->packets, w->compute_ns / 1e9, w->submit_ns / 1e9,
                    w->collect_ns / 1e9, w->write_ns / 1e9, w->meta_ns / 1e9, w->output_ns / 1e9,
                    (unsigned long long)w->polls, (unsigned long long)w->blocks,
                    w->cycle_blocks ? (double)w->dsp_cycles / w->cycle_blocks / 640 : 0);
            fflush(events);
            trim_log();
            pthread_mutex_unlock(&log_lock);
            report = ns();
        }
    }
    return NULL;
}
static void signal_handler(int s)
{
    (void)s;
    interrupted = 1;
}
int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: ble-receiver EVENTS.jsonl SECONDS\n");
        return 2;
    }
    char *end;
    unsigned long seconds = strtoul(argv[2], &end, 10);
    if (*end || !seconds || seconds > 86400)
        return 2;
    event_path = argv[1];
    events = fopen(event_path, "w");
    if (!events) {
        perror("events");
        return 1;
    }
    fprintf(events, "{\"type\":\"start\",\"sample_rate\":80000000}\n");
    fflush(events);
    fd = open("/dev/w1700k-sdr", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        perror("device");
        return 1;
    }
    struct w1700k_sdr_stream_rx g = {0};
    if (ioctl(fd, W1700K_SDR_STREAM_RX, &g) || g.slice_bytes != 8192 ||
        g.slots + 1024 >= g.slices) {
        perror("geometry");
        return 1;
    }
    size_t size = g.header_bytes + (size_t)g.slices * g.slice_bytes;
    void *map = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    ring = map;
    data = (unsigned char *)map + g.header_bytes;
    first_seq = completed();
    struct sigaction sa = {.sa_handler = signal_handler};
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigset_t stops;
    sigemptyset(&stops);
    sigaddset(&stops, SIGTERM);
    sigaddset(&stops, SIGINT);
    pthread_sigmask(SIG_BLOCK, &stops, NULL);
    pthread_t threads[3], feeders[3];
    static struct worker workers[3];
    for (unsigned b = 0; b < 3; b++) {
        workers[b].core = b;
        pthread_mutex_init(&workers[b].queue_lock, NULL);
        pthread_cond_init(&workers[b].changed, NULL);
        if (pthread_create(&feeders[b], NULL, feed_dsp, &workers[b]))
            return 1;
        if (pthread_create(&threads[b], NULL, consume, &workers[b]))
            return 1;
    }
    pthread_sigmask(SIG_UNBLOCK, &stops, NULL);
    cpu_set_t cpus;
    CPU_ZERO(&cpus);
    CPU_SET(3, &cpus);
    pthread_setaffinity_np(pthread_self(), sizeof(cpus), &cpus);
    struct w1700k_sdr_stream_rx req = {.timeout_ms = (uint32_t)seconds * 1000,
                                       .idle_ms = 1000,
                                       .go_address = 0x0090f008,
                                       .go_value = 0x474f474f,
                                       .stop_address = 0x0090f008,
                                       .stop_value = 0x53544f50};
    atomic_store(&stop_at, ns() + (uint64_t)seconds * 1000000000);
    int rc = ioctl(fd, W1700K_SDR_STREAM_RX, &req), error = rc ? errno : 0;
    atomic_store(&done, 1);
    for (unsigned b = 0; b < 3; b++) {
        pthread_mutex_lock(&workers[b].queue_lock);
        pthread_cond_broadcast(&workers[b].changed);
        pthread_mutex_unlock(&workers[b].queue_lock);
    }
    for (unsigned b = 0; b < 3; b++) {
        pthread_join(threads[b], NULL);
        pthread_join(feeders[b], NULL);
    }
    for (unsigned b = 0; b < 3; b++) {
        /* The producer has retired; collect any completed final block once. */
        (void)collect(&workers[b]);
        fprintf(events,
                "{\"type\":\"tail\",\"channel\":%u,\"raw_samples\":%llu,\"narrow_samples\":%u}\n",
                37 + b, (unsigned long long)((completed() - workers[b].next) * 2048),
                workers[b].used + (workers[b].head - workers[b].tail) * 2048 +
                    (workers[b].inflight ? 2048 : 0));
    }
    fprintf(events, "{\"type\":\"stopped\",\"errno\":%d,\"malformed\":%u,\"failed\":%d}\n", error,
            req.malformed, atomic_load(&failed));
    fclose(events);
    munmap(map, size);
    close(fd);
    return (error && error != EINTR) || req.malformed || atomic_load(&failed);
}
