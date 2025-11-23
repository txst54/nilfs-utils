#include "../include/workloader.h"

void run_append_writer(const config_t* cfg) {
    char filepath[4096];
    snprintf(filepath, sizeof(filepath), "%s/append_log.bin", cfg->path);

    // Open (or create) file for append
    int fd = open(filepath, O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
        perror("open");
        exit(1);
    }

    // Move to end-of-file (append position)
    off_t cur_size = lseek(fd, 0, SEEK_END);
    if (cur_size < 0) {
        perror("lseek");
        exit(1);
    }

    size_t bs = cfg->block_size;

    // Allocate deterministic write buffer
    char *buf = malloc(bs);
    if (!buf) {
        perror("malloc");
        exit(1);
    }
    for (size_t i = 0; i < bs; i++)
        buf[i] = (char)((i + cfg->seed) & 0xFF);

    uint64_t bytes_written = 0;
    uint64_t size_limit    = cfg->size_bytes;
    uint64_t duration_limit = cfg->duration_sec;
    uint64_t rate_limit     = cfg->rate_bytes;

    uint64_t start_time = now_usec();
    uint64_t next_report = start_time + 1000000ULL;

    // Token bucket
    uint64_t bucket = rate_limit;
    uint64_t last_refill = start_time;

    printf("Append writer: %s (starting at offset %lu)\n",
           filepath, (uint64_t)cur_size);

    while (1) {
        uint64_t now = now_usec();

        // Duration stop
        if ((now - start_time) / 1000000ULL >= duration_limit)
            break;

        // Size stop (logical growing limit)
        if ((uint64_t)cur_size + bytes_written >= size_limit)
            break;

        // Rate limiting
        if (now - last_refill >= 1000000ULL) {
            bucket = rate_limit;
            last_refill = now;
        }
        if (bucket < bs) {
            usleep(1000);
            continue;
        }

        // Write at end-of-file (append)
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

        // Status report
        if (now >= next_report) {
            printf("[append] written %lu MB (current file size %lu MB)\n",
                   bytes_written / (1024 * 1024),
                   (uint64_t)(cur_size + bytes_written) / (1024 * 1024));
            next_report = now + 1000000ULL;
        }
    }

    // Final sync if needed
    if (strcmp(cfg->fsync_mode, "none") != 0) {
        fsync(fd);
    }

    close(fd);
    free(buf);

    printf("Append writer finished. Final file size: %lu MB\n",
           (uint64_t)(cur_size + bytes_written) / (1024 * 1024));
}
