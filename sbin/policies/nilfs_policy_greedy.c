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
  ssize_t live_blocks;
  if (nilfs_get_live_blk(cleanerd, sustat, segnum, &live_blocks) == 0 
    || live_blocks < 0) {
    return 0; // segment is clean or error, not eligible
  }
	uint32_t blocks_per_segment = nilfs_get_blocks_per_segment(cleanerd->nilfs);
	
	if (si->sui_lastmod >= prottime && si->sui_lastmod <= now)
		return 0;  /* Protected */

	double util = (double)live_blocks / (double)blocks_per_segment;
	double UTIL_THRESHOLD = 0.60;  /* 60% live blocks allowed */

	if (util > UTIL_THRESHOLD) {
		return 0;  /* Too full → skip segment */
	}
	
	/* Greedy policy: score is number of reclaimable blocks */
	/* Reclaimable = Total - Live */
	candidate->segnum = segnum;
	candidate->score = (double)(blocks_per_segment - live_blocks);
	candidate->metadata = NULL;
  candidate->util = util;
	
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
