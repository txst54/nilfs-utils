#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <syslog.h>

/* Include NILFS headers BEFORE the policy header */
#include "nilfs.h"
#include "nilfs_gc.h"
#include "nilfs_cleaner.h"

#include "nilfs_cleaning_policy.h"
#include "cleanerd.h"

/* Original comparison function */
static int timestamp_compare(const void *elem1, const void *elem2)
{
	const struct nilfs_segment_candidate *c1 = elem1;
	const struct nilfs_segment_candidate *c2 = elem2;
	
	if (c1->score < c2->score)
		return -1;
	else if (c1->score > c2->score)
		return 1;
	
	return (c1->segnum < c2->segnum) ? -1 : 1;
}

/* Evaluate a single segment */
static int timestamp_evaluate(struct nilfs_cleaning_policy *policy,
			      struct nilfs_cleanerd *cleanerd,
			      const struct nilfs_sustat *sustat,
			      const struct nilfs_suinfo *si,
			      uint64_t segnum,
			      int64_t now,
			      int64_t prottime,
			      struct nilfs_segment_candidate *candidate)
{
	int64_t lastmod = si->sui_lastmod;
	int64_t thr = sustat->ss_nongc_ctime;
	int64_t imp;
	
	/* Timestamp policy logic */
	imp = lastmod <= now ? lastmod : thr - 1;
	
	if (imp >= thr)
		return 0;  /* Not eligible */
	
	if (lastmod >= prottime && lastmod <= now)
		return 0;  /* Protected */
	
	/* Set candidate info */
	candidate->segnum = segnum;
	candidate->score = -imp;  /* Negative so older = higher score */
	candidate->metadata = NULL;
	
	return 1;  /* Eligible */
}

/* Policy definition */
struct nilfs_cleaning_policy nilfs_policy_timestamp = {
	.name = "timestamp",
	.init = NULL,
	.destroy = NULL,
	.evaluate_segment = timestamp_evaluate,
	.compare = timestamp_compare,
	.select = NULL,  /* Use default selection logic */
	.policy_data = NULL
};