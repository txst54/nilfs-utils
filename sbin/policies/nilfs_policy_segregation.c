#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>

#include "nilfs.h"
#include "nilfs_gc.h"
#include "nilfs_cleaner.h"
#include "nilfs_cleaning_policy.h"
#include "cnormap.h"
#include "cleanerd.h"

/* * Configuration Constants */
#define HC_HOT_THRESHOLD_SEC  (24 * 60 * 60)  /* 24 Hours: Data younger than this is HOT */
#define HC_MIN_FILL_RATE      0.90            /* We want to fill 90% of a new segment */

/* Policy specific data structures */
struct hc_policy_data {
    uint32_t blocks_per_segment;
    int64_t hot_threshold; 
};

struct hc_candidate_meta {
    int is_hot;           /* 1 if Hot, 0 if Cold */
    uint32_t live_blocks; /* Number of live blocks to be moved */
    int64_t lastmod; 
};

/* * Comparison Function */
static int hc_compare(const void *elem1, const void *elem2)
{
    const struct nilfs_segment_candidate *c1 = elem1;
    const struct nilfs_segment_candidate *c2 = elem2;

    if (c1->score > c2->score)
        return -1;
    else if (c1->score < c2->score)
        return 1;

    return (c1->segnum < c2->segnum) ? -1 : 1;
}

/* Initialization */
static int hc_init(struct nilfs_cleaning_policy *policy,
                   struct nilfs_cleanerd *cleanerd)
{
    struct hc_policy_data *data;

    data = malloc(sizeof(*data));
    if (!data)
        return -ENOMEM;

    /* Cache filesystem geometry */
    data->blocks_per_segment = nilfs_get_blocks_per_segment(cleanerd->nilfs);
    data->hot_threshold = HC_HOT_THRESHOLD_SEC;

    policy->policy_data = data;
    return 0;
}

/* Cleanup */
static void hc_destroy(struct nilfs_cleaning_policy *policy)
{
    if (policy->policy_data) {
        free(policy->policy_data);
        policy->policy_data = NULL;
    }
}

/* Evaluation */
/* Note: This function is called manually by strict_cluster_select.
 * It does not calculate a score because we sort by Age, not Score.
 * Its primary purpose is to fetch metadata (live blocks, timestamp).
 */
static int hc_evaluate(struct nilfs_cleaning_policy *policy,
                       struct nilfs_cleanerd *cleanerd,
                       const struct nilfs_sustat *sustat,
                       const struct nilfs_suinfo *si,
                       uint64_t segnum,
                       int64_t now,
                       int64_t prottime,
                       struct nilfs_segment_candidate *candidate)
{
    struct hc_policy_data *pdata = (struct hc_policy_data *)policy->policy_data;
    struct nilfs *nilfs = cleanerd->nilfs;
    struct nilfs_reclaim_stat stat;
    nilfs_cno_t protcno;
    int ret;

    /* 1. Track back to find protection checkpoint */
    ret = nilfs_cnormap_track_back(cleanerd->cnormap, 0, &protcno);
    if (ret < 0) {
      syslog(LOG_ERR, "Error getting protection checkpoint number");
      return 0;
    }

    memset(&stat, 0, sizeof(stat));

    /* 2. Assess if dirty and get live block count */
    ret = assess_segment_if_dirty(nilfs, sustat, segnum, protcno, &stat);
    
    if (ret <= 0) {
      syslog(LOG_INFO, "Segment %lu is clean or error during assessment", segnum);
      return 0; /* Clean or Error */
    }
    if (stat.live_blks < 0) {
      syslog(LOG_ERR, "Error assessing live blocks or protected for segment %lu", segnum);
      return 0; /* Protected */
    }

    /* 3. Check Protocol Time Protection */
    if (si->sui_lastmod >= prottime && si->sui_lastmod <= now) {
        syslog(LOG_INFO, "Segment %lu is protected (lastmod: %ld)", segnum, si->sui_lastmod);
        return 0;
    }

    if (!nilfs_suinfo_reclaimable(si)) {
        syslog(LOG_INFO, "Segment %lu is not reclaimable", segnum);
        return 0; /* Not reclaimable */
    }

    int64_t age = now - si->sui_lastmod;
    if (age < 0) age = 0;

    /* 4. Allocate Metadata for this candidate */
    struct hc_candidate_meta *meta = malloc(sizeof(*meta));
    if (!meta) {
      syslog(LOG_ERR, "OOM allocating candidate metadata");
      return 0; // Skip if OOM
    }

    /* 5. Classify: Hot vs Cold */
    meta->live_blocks = (uint32_t)stat.live_blks;
    meta->is_hot = (age < pdata->hot_threshold);
    meta->lastmod = si->sui_lastmod;

    /* 6. Set Score (Unused by this policy, but set for safety) */
    candidate->segnum = segnum;
    candidate->score = 0.0; /* We don't use score-based sorting */
    candidate->metadata = meta;

    return 1; /* Eligible */
}

static int age_compare_descending(const void *elem1, const void *elem2)
{
    const struct nilfs_segment_candidate *c1 = elem1;
    const struct nilfs_segment_candidate *c2 = elem2;

    /* Retrieve the metadata where we stored the timestamp */
    const struct hc_candidate_meta *m1 = (const struct hc_candidate_meta *)c1->metadata;
    const struct hc_candidate_meta *m2 = (const struct hc_candidate_meta *)c2->metadata;

    if (m1->lastmod < m2->lastmod)
        return -1; /* c1 is older (smaller TS), so it goes first */
    
    if (m1->lastmod > m2->lastmod)
        return 1;  /* c1 is younger, so it goes later */

    if (c1->segnum < c2->segnum)
        return -1;
    if (c1->segnum > c2->segnum)
        return 1;

    return 0;
}

/* * STRICT AGE CLUSTERING SELECTION 
 * This ensures that we never mix Old and Young segments in the same output.
 */
static ssize_t strict_cluster_select(struct nilfs_cleaning_policy *policy,
                         struct nilfs_cleanerd *cleanerd,
                         struct nilfs_sustat *sustat,
                         int64_t now,
                         uint64_t *segnums,
                         int64_t prottime)
{
    struct nilfs *nilfs = cleanerd->nilfs;
    struct nilfs_segment_candidate *candidates = NULL;
    struct nilfs_suinfo si;
    unsigned long nsegs;
    uint64_t segnum;
    ssize_t count = 0;
    ssize_t candidate_count = 0;
    ssize_t capacity = 0;
    int ret;
    struct timeval tv;
    
    /* 1. Get total number of segments to iterate */
    nsegs = nilfs_get_nsegments(nilfs);
    if (nsegs == 0) return 0;

    /* Allocate initial capacity for candidates */
    capacity = 100; /* Start small */
    candidates = malloc(capacity * sizeof(struct nilfs_segment_candidate));
    if (!candidates) return -ENOMEM;
    syslog(LOG_INFO, "Hot-Cold Segregation: Scanning %lu segments", nsegs);
    /* 2. Manual Iteration over all segments */
    for (segnum = 0; segnum < nsegs; segnum++) {
        /* Fetch Segment Usage Info */
        ret = nilfs_get_suinfo(nilfs, segnum, &si, 1);
        if (ret < 0) continue; 

        /* Check eligibility using our evaluate function */
        struct nilfs_segment_candidate cand;
        memset(&cand, 0, sizeof(cand));
        
        /* Note: 'prottime' is usually passed via pointer, we dereference if available */

        ret = hc_evaluate(policy, cleanerd, sustat, &si, segnum, now, prottime, &cand);
        
        if (ret > 0) {
            syslog(LOG_INFO, "  Eligible Segment %lu classified as %s with TS %ld", segnum,
                   ((struct hc_candidate_meta *)cand.metadata)->is_hot ? "HOT" : "COLD", 
                   ((struct hc_candidate_meta *)cand.metadata)->lastmod);
            /* Add to array, resize if needed */
            if (candidate_count >= capacity) {
                capacity *= 2;
                struct nilfs_segment_candidate *tmp = realloc(candidates, capacity * sizeof(struct nilfs_segment_candidate));
                if (!tmp) {
                    /* Handle OOM: Clean up existing metadata and fail */
                    ssize_t j;
                    for (j = 0; j < candidate_count; j++) free(candidates[j].metadata);
                    free(candidates);
                    return -ENOMEM;
                }
                candidates = tmp;
            }
            candidates[candidate_count++] = cand;
        }
    }

    /* 3. Sort by Age (Oldest to Newest) */
    if (candidate_count > 0) {
        qsort(candidates, candidate_count, sizeof(struct nilfs_segment_candidate), age_compare_descending);
    }

    /* 4. Cluster Selection Logic */
    if (candidate_count > 0) {
        /* Pick the 'Best' Seed Candidate (Oldest) */
        struct nilfs_segment_candidate *seed = &candidates[0];
        uint64_t accum_blocks = 0;
        uint32_t blocks_per_seg = nilfs_get_blocks_per_segment(nilfs);
        int64_t age_window = 4; // 1 Day Window allowed

        int64_t seed_ts = ((struct hc_candidate_meta *)seed->metadata)->lastmod; 
        syslog(LOG_INFO, "Hot-Cold Segregation: Seed Segment %lu with TS %ld", seed->segnum, seed_ts);

        ssize_t i;
        for (i = 0; i < candidate_count; i++) {
            if (count >= NILFS_CLDCONFIG_NSEGMENTS_PER_CLEAN_MAX) {
                syslog(LOG_WARNING, "Reached maximum selection limit (%ld segments)", 
                       NILFS_CLDCONFIG_NSEGMENTS_PER_CLEAN_MAX);
                break; /* Reached desired number of segments */
            }
            struct nilfs_segment_candidate *cand = &candidates[i];
            int64_t cand_ts = ((struct hc_candidate_meta *)cand->metadata)->lastmod;
            syslog(LOG_INFO, "  Considering Segment %lu with TS %ld", cand->segnum, cand_ts);

            /* CHECK: Is this candidate within the time window of the seed? */
            if (llabs(seed_ts - cand_ts) > age_window) {
                /* Since we are sorted by age, we might hit younger segments.
                   If strict segregation is desired, we skip. */
                continue; 
            }
            syslog(LOG_INFO, "    --> Selected Segment %lu, placing at count %lu", cand->segnum, count);
            /* Add to selection output array 'segnums' */
            segnums[count++] = cand->segnum;
            syslog(LOG_INFO, "    Added Segment %lu to selection (Total selected: %ld)", cand->segnum, count);
            
            /* Accumulate live blocks */
            struct hc_candidate_meta *meta = (struct hc_candidate_meta *)cand->metadata;
            accum_blocks += meta->live_blocks;
            syslog(LOG_INFO, "    Accumulated live blocks: %lu / %u", accum_blocks, blocks_per_seg);

            /* Stop if we have filled a new segment */
            if (accum_blocks >= blocks_per_seg) {
                break;
            }
        }
    }

    /* 5. Cleanup local candidate array and metadata */
    ssize_t i;
    for (i = 0; i < candidate_count; i++) {
        if (candidates[i].metadata)
            free(candidates[i].metadata);
    }
    free(candidates);

    return count;
}

/* Policy Definition */
struct nilfs_cleaning_policy nilfs_policy_hot_cold = {
    .name = "segregation",
    .init = hc_init,
    .destroy = hc_destroy,
    .evaluate_segment = hc_evaluate,
    .compare = hc_compare,
    .select = strict_cluster_select, /* We override the default selection */
    .policy_data = NULL
};

/* Registration */
int nilfs_register_policy_hot_cold(void)
{
    return nilfs_register_policy(&nilfs_policy_hot_cold);
}