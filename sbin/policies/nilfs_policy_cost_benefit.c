#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "nilfs.h"
#include "nilfs_gc.h"
#include "nilfs_cleaner.h"
#include "nilfs_cleaning_policy.h"
#include "cleanerd.h"

/* Cost-Benefit comparison function: higher score first */
static int cb_compare(const void *elem1, const void *elem2)
{
	const struct nilfs_segment_candidate *c1 = elem1;
	const struct nilfs_segment_candidate *c2 = elem2;
	
	if (c1->score > c2->score)
		return -1;
	else if (c1->score < c2->score)
		return 1;
	
	return (c1->segnum < c2->segnum) ? -1 : 1;
}

/* Evaluate a single segment for Cost-Benefit policy */
static int cb_evaluate(struct nilfs_cleaning_policy *policy,
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
	double u = (double)live_blocks / blocks_per_segment;
	int64_t age = now - si->sui_lastmod;
	
	if (si->sui_lastmod >= prottime && si->sui_lastmod <= now)
		return 0;  /* Protected */
	
	if (age < 0)
		age = 0; /* Should not happen but safety check */

	/* Cost-Benefit formula: (1 - u) * age / (1 + u) */
	/* (1 - u) is the benefit (reclaimable space ratio) */
	/* (1 + u) is the cost (read + write cost factor) */
	/* age is the stability factor */
	
	candidate->segnum = segnum;
	candidate->score = (1.0 - u) * age / (1.0 + u);
	candidate->metadata = NULL;
  candidate->util = u;
		return 1;  /* Eligible */
}

/* Policy definition */
struct nilfs_cleaning_policy nilfs_policy_cost_benefit = {
	.name = "cost-benefit",
	.init = NULL,
	.destroy = NULL,
	.evaluate_segment = cb_evaluate,
	.compare = cb_compare,
	.select = NULL,
	.policy_data = NULL
};
