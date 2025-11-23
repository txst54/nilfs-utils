#include "../include/workloader.h"

void run_seq_writer(const config_t* cfg) {
    char filepath[4096];
    snprintf(filepath, sizeof(filepath), "%s/seq_write.bin", cfg->path);

    int fd = open(filepath, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) {
        perror("open");
        exit(1);
    }

    // Allocate write buffer
    size_t bs = cfg->block_size;
    char *buf = malloc(bs);
    if (!buf) {
        perror("malloc");
        exit(1);
    }

    // Fill buffer with deterministic pseudo-data
    for (size_t i = 0; i < bs; i++) {
        buf[i] = (char)((i + cfg->seed) & 0xFF);
    }

    uint64_t bytes_written = 0;
    uint64_t size_limit    = cfg->size_bytes;
    uint64_t duration_limit = cfg->duration_sec;
    uint64_t rate_limit     = cfg->rate_bytes;

    uint64_t start_time = now_usec();
    uint64_t next_report = start_time + 1ULL * 1000000ULL;

    // Token bucket rate control
    uint64_t bucket = rate_limit;  // bytes available this second
    uint64_t last_refill = start_time;

    printf("Sequential writer: %s\n", filepath);

    while (1) {
        uint64_t now = now_usec();

        // Duration end condition
        if (((now - start_time) / 1000000ULL) >= duration_limit)
            break;

        // Size end condition
        if (bytes_written >= size_limit)
            break;

        // Refill token bucket every second
        if (now - last_refill >= 1000000ULL) {
            bucket = rate_limit;
            last_refill = now;
        }

        // If empty bucket, sleep briefly
        if (bucket < bs) {
            usleep(1000); // 1ms granularity is fine
            continue;
        }

        ssize_t w = write(fd, buf, bs);
        if (w < 0) {
            perror("write");
            exit(1);
        }

        bytes_written += w;
        bucket -= w;

        // FSYNC policy
        if (strcmp(cfg->fsync_mode, "always") == 0) {
            fsync(fd);
        } else if (strcmp(cfg->fsync_mode, "periodic") == 0) {
            if ((bytes_written / bs) % cfg->fsync_interval == 0)
                fsync(fd);
        }

        // Status
        if (now >= next_report) {
            printf("[seq] written %lu MB\n", bytes_written / (1024 * 1024));
            next_report = now + 1000000ULL;
        }
    }

    if (strcmp(cfg->fsync_mode, "none") != 0) {
        fsync(fd);
    }

    close(fd);
    free(buf);

    printf("Sequential writer finished. Total: %lu MB\n",
           bytes_written / (1024 * 1024));
}
