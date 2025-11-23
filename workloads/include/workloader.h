#ifndef WORKLOADER_H
#define WORKLOADER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "utilites.h"


typedef enum {
    TEST_SEQ,
    TEST_RAND,
    TEST_APPEND,
    TEST_HOTSPOT,
    TEST_METADATA_STORM,
    TEST_UNKNOWN
} test_type_t;

typedef struct {
    test_type_t test;

    uint64_t size_bytes;
    uint64_t rate_bytes;
    uint64_t duration_sec;

    uint64_t block_size;
    int threads;
    uint64_t window_size;

    const char *path;
    const char *log_path;

    const char *fsync_mode;
    int fsync_interval;

    uint64_t seed;
} config_t;

void run_seq_writer(const config_t *);
void run_rand_writer(const config_t *);
void run_append_writer(const config_t *);
void run_hotspot_writer(const config_t *);
void run_metadata_storm(const config_t *);

#endif
