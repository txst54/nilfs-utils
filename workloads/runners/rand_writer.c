#include "../include/workloader.h"

// Simple deterministic xorshift64* RNG
static uint64_t xorshift64star(uint64_t *state) {
    uint64_t x = *state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *state = x;
    return x * 2685821657736338717ULL;
}

void run_rand_writer(const config_t* cfg) {
    char filepath[4096];
    snprintf(filepath, sizeof(filepath), "%s/rand_write.bin", cfg->path);

    int fd = open(filepath, O_CREAT | O_RDWR, 0644);
    if (fd < 0) {
        perror("open");
        exit(1);
    }

    // Pre-extend the file to size_bytes
    if (ftruncate(fd, cfg->size_bytes) < 0) {
        perror("ftruncate");
        exit(1);
    }

    size_t bs = cfg->block_size;
    uint64_t window_size = cfg->window_size;
    if (window_size > cfg->size_bytes)
        window_size = cfg->size_bytes;

    char *buf = malloc(bs);
    if (!buf) {
        perror("malloc");
        exit(1);
    }

    // Fill buffer deterministically
    for (size_t i = 0; i < bs; i++) {
        buf[i] = (char)((i + cfg->seed) & 0xFF);
    }

    uint64_t bytes_written = 0;
    uint64_t duration_limit = cfg->duration_sec;
    uint64_t rate_limit = cfg->rate_bytes;

    uint64_t start_time = now_usec();
    uint64_t next_report = start_time + 1ULL * 1000000ULL;

    // Token bucket rate limiter
    uint64_t bucket = rate_limit;
    uint64_t last_refill = start_time;

    // RNG state
    uint64_t rng = cfg->seed ? cfg->seed : 0x123456789ABCDEFULL;

    printf("Random writer: %s (window=%lu bytes)\n",
           filepath, window_size);

    while (1) {
        uint64_t now = now_usec();

        // Duration expiration
        if ((now - start_time) / 1000000ULL >= duration_limit)
            break;

        // Refill rate bucket
        if (now - last_refill >= 1000000ULL) {
            bucket = rate_limit;
            last_refill = now;
        }

        if (bucket < bs) {
            usleep(1000);
            continue;
        }

        // Pick random offset inside window
        uint64_t r = xorshift64star(&rng);
        uint64_t offset = (r % (window_size / bs)) * bs;

        ssize_t w = pwrite(fd, buf, bs, offset);
        if (w < 0) {
            perror("pwrite");
            exit(1);
        }

        bytes_written += w;
        bucket -= w;

        // FSYNC rules
        if (strcmp(cfg->fsync_mode, "always") == 0) {
            fsync(fd);
        } else if (strcmp(cfg->fsync_mode, "periodic") == 0) {
            if ((bytes_written / bs) % cfg->fsync_interval == 0)
                fsync(fd);
        }

        // Status report
        if (now >= next_report) {
            printf("[rand] written %lu MB (random overwrites)\n",
                   bytes_written / (1024 * 1024));
            next_report = now + 1000000ULL;
        }
    }

    if (strcmp(cfg->fsync_mode, "none") != 0) {
        fsync(fd);
    }

    close(fd);
    free(buf);

    printf("Random writer finished. Total: %lu MB random writes\n",
           bytes_written / (1024 * 1024));
}
