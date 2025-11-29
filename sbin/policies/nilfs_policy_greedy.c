#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "nilfs.h"
#include "nilfs_gc.h"
#include "nilfs_cleaner.h"
#include "nilfs_cleaning_policy.h"
#include "cleanerd.h"

/* Greedy comparison function: higher score (more reclaimable) first */
static int greedy_compare(const void *elem1, const void *elem2)
{
	const struct nilfs_segment_candidate *c1 = elem1;
	const struct nilfs_segment_candidate *c2 = elem2;
	
	if (c1->score > c2->score)
		return -1;
	else if (c1->score < c2->score)
		return 1;
	
	return (c1->segnum < c2->segnum) ? -1 : 1;
}

/* Evaluate a single segment for Greedy policy */
static int greedy_evaluate(struct nilfs_cleaning_policy *policy,
			   struct nilfs_cleanerd *cleanerd,
			   const struct nilfs_sustat *sustat,
			   const struct nilfs_suinfo *si,
			   uint64_t segnum,
			   int64_t now,
			   int64_t prottime,
			   struct nilfs_segment_candidate *candidate)
{
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
  ssize_t live_blocks = stat.live_blks;
	uint32_t blocks_per_segment = nilfs_get_blocks_per_segment(cleanerd->nilfs);
	
	if (si->sui_lastmod >= prottime && si->sui_lastmod <= now)
		return 0;  /* Protected */
	
	/* Greedy policy: score is number of reclaimable blocks */
	/* Reclaimable = Total - Live */
	candidate->segnum = segnum;
	candidate->score = (double)(blocks_per_segment - live_blocks);
	candidate->metadata = NULL;
	
	return 1;  /* Eligible */
}

/* Policy definition */
struct nilfs_cleaning_policy nilfs_policy_greedy = {
	.name = "greedy",
	.init = NULL,
	.destroy = NULL,
	.evaluate_segment = greedy_evaluate,
	.compare = greedy_compare,
	.select = NULL,
	.policy_data = NULL
};
