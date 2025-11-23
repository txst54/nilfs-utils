#include "../include/workloader.h"

void run_hot_cold_writer(const config_t* cfg) {
    char filepath[4096];
    snprintf(filepath, sizeof(filepath), "%s/hot_cold.bin", cfg->path);

    int fd = open(filepath, O_CREAT | O_RDWR, 0644);
    if (fd < 0) {
        perror("open");
        exit(1);
    }

    // Pre-extend entire file
    if (ftruncate(fd, cfg->size_bytes) < 0) {
        perror("ftruncate");
        exit(1);
    }

    size_t bs = cfg->block_size;

    // Hot window is cfg->window_size, cold window is the rest
    uint64_t hot_window = cfg->window_size;
    if (hot_window > cfg->size_bytes)
        hot_window = cfg->size_bytes;

    uint64_t cold_window = cfg->size_bytes - hot_window;
    uint64_t hot_slots = hot_window / bs;
    uint64_t cold_slots = cold_window / bs;

    if (hot_slots == 0 || cold_slots == 0) {
        fprintf(stderr, "Hot/cold windows too small.\n");
        exit(1);
    }

    uint64_t hot_base = 0;
    uint64_t cold_base = hot_window;

    // Allocate deterministic buffer
    char *buf = malloc(bs);
    if (!buf) {
        perror("malloc");
        exit(1);
    }
    for (size_t i = 0; i < bs; i++)
        buf[i] = (char)((i + cfg->seed) & 0xFF);

    uint64_t bytes_written = 0;
    uint64_t duration_limit = cfg->duration_sec;
    uint64_t rate_limit = cfg->rate_bytes;

    uint64_t start_time = now_usec();
    uint64_t next_report = start_time + 1000000ULL;

    // Token bucket
    uint64_t bucket = rate_limit;
    uint64_t last_refill = start_time;

    // RNG
    uint64_t rng = cfg->seed ? cfg->seed : 0xFEEDBEEFC0FFEE11ULL;

    printf("Hot-Cold writer: %s\n", filepath);
    printf("  Hot window:  %lu MB (80%% writes)\n", hot_window / (1024*1024));
    printf("  Cold window: %lu MB (20%% writes)\n", cold_window / (1024*1024));

    while (1) {
        uint64_t now = now_usec();

        // Duration expiration
        if ((now - start_time) / 1000000ULL >= duration_limit)
            break;

        // Rate bucket refill
        if (now - last_refill >= 1000000ULL) {
            bucket = rate_limit;
            last_refill = now;
        }
        if (bucket < bs) {
            usleep(1000);
            continue;
        }

        // Random choice: 80% hot, 20% cold
        uint64_t r = xorshift64star(&rng);
        uint64_t offset;

        if ((r % 100) < 80) {   // 80% HOT
            uint64_t hot_idx = (r >> 32) % hot_slots;
            offset = hot_base + hot_idx * bs;
        } else {                // 20% COLD
            uint64_t cold_idx = (r >> 32) % cold_slots;
            offset = cold_base + cold_idx * bs;
        }

        ssize_t w = pwrite(fd, buf, bs, offset);
        if (w < 0) {
            perror("pwrite");
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

        if (now >= next_report) {
            printf("[hot_cold] written %lu MB\n",
                   bytes_written / (1024*1024));
            next_report = now + 1000000ULL;
        }
    }

    if (strcmp(cfg->fsync_mode, "none") != 0)
        fsync(fd);

    close(fd);
    free(buf);

    printf("Hot-Cold writer finished. Total: %lu MB\n",
           bytes_written / (1024*1024));
}
