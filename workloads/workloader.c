#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>
#include <errno.h>

#include "include/workloader.h"

// helper parsers
static uint64_t parse_size(const char *s) {
    char *end;
    uint64_t val = strtoull(s, &end, 10);
    if (end == s) {
        fprintf(stderr, "Invalid size: %s\n", s);
        exit(1);
    }

    switch (toupper(*end)) {
        case 'K': val *= 1024ULL; break;
        case 'M': val *= 1024ULL * 1024ULL; break;
        case 'G': val *= 1024ULL * 1024ULL * 1024ULL; break;
        case 'T': val *= 1024ULL * 1024ULL * 1024ULL * 1024ULL; break;
        case '\0': break;  // bytes
        default:
            fprintf(stderr, "Unknown size suffix: %c\n", *end);
            exit(1);
    }

    return val;
}

static uint64_t parse_time(const char *s) {
    char *end;
    uint64_t val = strtoull(s, &end, 10);
    if (end == s) {
        fprintf(stderr, "Invalid duration: %s\n", s);
        exit(1);
    }

    switch (tolower(*end)) {
        case 's': return val;
        case 'm': return val * 60;
        case 'h': return val * 3600;
        case '\0': return val; // seconds
        default:
            fprintf(stderr, "Unknown duration suffix: %c\n", *end);
            exit(1);
    }
}

static test_type_t parse_test(const char *s) {
    if (strcmp(s, "seq") == 0) return TEST_SEQ;
    if (strcmp(s, "rand") == 0) return TEST_RAND;
    if (strcmp(s, "append") == 0) return TEST_APPEND;
    if (strcmp(s, "hotspot") == 0) return TEST_HOTSPOT;
    if (strcmp(s, "storm") == 0) return TEST_METADATA_STORM;
    if (strcmp(s, "hotcold") == 0) return TEST_HOT_COLD;
    return TEST_UNKNOWN;
}

// Default config
static void load_defaults(config_t *cfg) {
    cfg->test = TEST_UNKNOWN;

    cfg->size_bytes = parse_size("100G");
    cfg->rate_bytes = parse_size("200M");
    cfg->duration_sec = parse_time("10m");

    cfg->block_size = parse_size("4K");
    cfg->threads = 1;
    cfg->window_size = parse_size("1G");

    cfg->path = ".";
    cfg->log_path = NULL;

    cfg->fsync_mode = "none";
    cfg->fsync_interval = 1000;

    cfg->seed = 0;
}

int main(int argc, char **argv) {
    config_t cfg;
    load_defaults(&cfg);

    static struct option long_opts[] = {
        {"test",           required_argument, 0,  0},
        {"size",           required_argument, 0,  0},
        {"rate",           required_argument, 0,  0},
        {"duration",       required_argument, 0,  0},
        {"path",           required_argument, 0,  0},
        {"bs",             required_argument, 0,  0},
        {"threads",        required_argument, 0,  0},
        {"window",         required_argument, 0,  0},
        {"log",            required_argument, 0,  0},
        {"fsync",          required_argument, 0,  0},
        {"fsync-interval", required_argument, 0,  0},
        {"seed",           required_argument, 0,  0},
        {0,                0,                 0,  0}
    };

    int opt, idx;
    while ((opt = getopt_long(argc, argv, "", long_opts, &idx)) != -1) {
        if (opt != 0) continue; // we use only long options

        const char *name = long_opts[idx].name;
        const char *val = optarg;

        if (strcmp(name, "test") == 0) {
            cfg.test = parse_test(val);
            if (cfg.test == TEST_UNKNOWN) {
                fprintf(stderr, "Unknown test type: %s\n", val);
                exit(1);
            }
        } else if (strcmp(name, "size") == 0) {
            cfg.size_bytes = parse_size(val);
        } else if (strcmp(name, "rate") == 0) {
            cfg.rate_bytes = parse_size(val);
        } else if (strcmp(name, "duration") == 0) {
            cfg.duration_sec = parse_time(val);
        } else if (strcmp(name, "path") == 0) {
            cfg.path = val;
        } else if (strcmp(name, "bs") == 0) {
            cfg.block_size = parse_size(val);
        } else if (strcmp(name, "threads") == 0) {
            cfg.threads = atoi(val);
        } else if (strcmp(name, "window") == 0) {
            cfg.window_size = parse_size(val);
        } else if (strcmp(name, "log") == 0) {
            cfg.log_path = val;
        } else if (strcmp(name, "fsync") == 0) {
            cfg.fsync_mode = val;
        } else if (strcmp(name, "fsync-interval") == 0) {
            cfg.fsync_interval = atoi(val);
        } else if (strcmp(name, "seed") == 0) {
            cfg.seed = strtoull(val, NULL, 10);
        }
    }

    // Validate required args
    if (cfg.test == TEST_UNKNOWN) {
        fprintf(stderr, "--test=<seq|rand|append|hotspot|storm> is required\n");
        exit(1);
    }

    // Show config summary
    printf("Workloader starting:\n");
    printf("  test         = %d\n", cfg.test);
    printf("  size         = %lu bytes\n", cfg.size_bytes);
    printf("  rate         = %lu bytes/sec\n", cfg.rate_bytes);
    printf("  duration     = %lu sec\n", cfg.duration_sec);
    printf("  path         = %s\n", cfg.path);
    printf("  block size   = %lu\n", cfg.block_size);
    printf("  threads      = %d\n", cfg.threads);
    printf("  window size  = %lu\n", cfg.window_size);
    printf("  fsync        = %s (%d)\n", cfg.fsync_mode, cfg.fsync_interval);
    printf("  seed         = %lu\n", cfg.seed);
    if (cfg.log_path)
        printf("  log          = %s\n", cfg.log_path);

    // Dispatch
    switch (cfg.test) {
        case TEST_SEQ:              run_seq_writer(&cfg); break;
        case TEST_RAND:             run_rand_writer(&cfg); break;
        case TEST_APPEND:           run_append_writer(&cfg); break;
        case TEST_HOTSPOT:          run_hotspot_writer(&cfg); break;
        case TEST_METADATA_STORM:   run_metadata_storm(&cfg); break;
        case TEST_HOT_COLD:         run_hot_cold_writer(&cfg); break;
        default:
            fprintf(stderr, "Invalid test type\n");
            exit(1);
    }

    return 0;
}
