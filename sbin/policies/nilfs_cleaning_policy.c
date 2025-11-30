#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "nilfs.h"
#include "nilfs_gc.h"
#include "nilfs_cleaner.h"
#include "nilfs_cleaning_policy.h"
#include "cleanerd.h"

int nilfs_get_live_blk(struct nilfs_cleanerd *cleanerd,
                         const struct nilfs_sustat *sustat,
                         uint64_t segnum, ssize_t *live_blocks) {
  struct nilfs_reclaim_stat stat;
  struct nilfs *nilfs = cleanerd->nilfs;
  nilfs_cno_t protcno;
  struct nilfs_cnormap *cnormap = cleanerd->cnormap;
  int ret;
  ret = nilfs_cnormap_track_back(cnormap, 0, &protcno);
  memset(&stat, 0, sizeof(stat));
  ret = assess_segment_if_dirty(
    nilfs,
    sustat,
    segnum,
    protcno,
    &stat
  );
  if (!ret) {
    return 0; // segment is clean, not eligible
  } else if (ret < 0) {
    syslog(LOG_ERR, "error assessing segment %llu", (unsigned long long)segnum);
    return 0; // on error, treat as not eligible
  }
  *live_blocks = stat.live_blks;
  return 1;
}