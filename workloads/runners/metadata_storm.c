#include "../include/workloader.h"

void run_metadata_storm(const config_t* cfg) {
    char rootdir[4096];
    snprintf(rootdir, sizeof(rootdir), "%s/metadata_storm", cfg->path);

    if (mkdir(rootdir, 0755) < 0 && errno != EEXIST) {
        perror("mkdir(rootdir)");
        exit(1);
    }

    // Create a few subdirectories to spread dentries around
    const int num_subdirs = 8;
    char subdirs[num_subdirs][4096];
    for (int i = 0; i < num_subdirs; i++) {
        snprintf(subdirs[i], sizeof(subdirs[i]),
                 "%s/d%02d", rootdir, i);
        if (mkdir(subdirs[i], 0755) < 0 && errno != EEXIST) {
            perror("mkdir(subdir)");
            exit(1);
        }
    }

    size_t bs = cfg->block_size;
    if (bs > 4096) {
        bs = 4096; // small writes for metadata storm
    }

    char *buf = malloc(bs);
    if (!buf) {
        perror("malloc");
        exit(1);
    }
    for (size_t i = 0; i < bs; i++)
        buf[i] = (char)((i + cfg->seed) & 0xFF);

    uint64_t duration_limit = cfg->duration_sec;
    uint64_t start_time = now_usec();
    uint64_t next_report = start_time + 1000000ULL;

    uint64_t rng = cfg->seed ? cfg->seed : 0xBADC0FFEE0DDF00DULL;

    uint64_t files_created = 0;
    uint64_t files_deleted = 0;
    uint64_t dirs_created  = 0;
    uint64_t dirs_removed  = 0;
    uint64_t renames       = 0;

    printf("Metadata storm under: %s (duration %lus)\n",
           rootdir, duration_limit);

    while (1) {
        uint64_t now = now_usec();
        if ((now - start_time) / 1000000ULL >= duration_limit)
            break;

        // Choose a random subdir
        uint64_t r = xorshift64star(&rng);
        int si = r % num_subdirs;
        const char *base = subdirs[si];

        // Generate a random file name
        char fname[4096], tmpname[4096], newname[4096];
        unsigned long id = (unsigned long)(r & 0xFFFFFFFFUL);

        snprintf(fname, sizeof(fname), "%s/f_%08lx", base, id);
        snprintf(tmpname, sizeof(tmpname), "%s/t_%08lx", base, id);
        snprintf(newname, sizeof(newname), "%s/g_%08lx", base, id);

        // Randomly choose operation mix:
        // 0,1: create+write; 2: unlink; 3: mkdir; 4: rmdir; 5: rename
        int op = r % 6;

        if (op == 0 || op == 1) {
            // create + small write
            int fd = open(fname, O_CREAT | O_WRONLY | O_TRUNC, 0644);
            if (fd >= 0) {
                ssize_t w = write(fd, buf, bs);
                (void)w;
                close(fd);
                files_created++;
            } else if (errno != ENOSPC) {
                // ignore ENOSPC gracefully, but abort on other errors
                perror("open(create)");
                exit(1);
            }
        } else if (op == 2) {
            // unlink
            if (unlink(fname) == 0) {
                files_deleted++;
            }
        } else if (op == 3) {
            // mkdir a nested dir
            char dname[4096];
            snprintf(dname, sizeof(dname), "%s/sub_%08lx", base, id);
            if (mkdir(dname, 0755) == 0) {
                dirs_created++;
            }
        } else if (op == 4) {
            // rmdir a nested dir (may fail if not empty)
            char dname[4096];
            snprintf(dname, sizeof(dname), "%s/sub_%08lx", base, id);
            if (rmdir(dname) == 0) {
                dirs_removed++;
            }
        } else if (op == 5) {
            // rename: tmp -> new, or fname -> new
            // create tmp if needed
            int fd = open(tmpname, O_CREAT | O_WRONLY | O_TRUNC, 0644);
            if (fd >= 0) {
                write(fd, buf, bs);
                close(fd);
                if (rename(tmpname, newname) == 0) {
                    renames++;
                } else {
                    // best-effort: ignore failures
                    unlink(tmpname);
                }
            } else if (errno != ENOSPC) {
                perror("open(rename tmp)");
                exit(1);
            }
        }

        if (now >= next_report) {
            printf("[storm] files +%lu/-%lu, dirs +%lu/-%lu, renames %lu\n",
                   files_created, files_deleted,
                   dirs_created, dirs_removed,
                   renames);
            next_report = now + 1000000ULL;
        }
    }

    free(buf);

    printf("Metadata storm finished.\n");
    printf("  files created:  %lu\n", files_created);
    printf("  files deleted:  %lu\n", files_deleted);
    printf("  dirs created:   %lu\n", dirs_created);
    printf("  dirs removed:   %lu\n", dirs_removed);
    printf("  renames:        %lu\n", renames);
}
