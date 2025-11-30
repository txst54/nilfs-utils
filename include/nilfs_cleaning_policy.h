#ifndef NILFS_CLEANING_POLICY_H
#define NILFS_CLEANING_POLICY_H

#include <stdint.h>
#include <sys/types.h>

struct nilfs_cleanerd;
struct nilfs_sustat;
struct nilfs_suinfo;

/**
 * struct nilfs_segment_candidate - segment cleaning candidate
 * @segnum: segment number
 * @score: policy-specific score (higher = better candidate)
 * @metadata: pointer to policy-specific metadata
 */
struct nilfs_segment_candidate {
	uint64_t segnum;
	double score;
	void *metadata;  /* Policy can store custom data here */
};

/**
 * struct nilfs_cleaning_policy - pluggable cleaning policy interface
 * @name: human-readable policy name
 * @init: initialize policy-specific state
 * @destroy: cleanup policy-specific state
 * @evaluate_segment: calculate score for a single segment
 * @compare: comparison function for sorting candidates
 * @select: optional: custom selection logic (overrides default)
 * @policy_data: pointer to policy-specific global state
 */
struct nilfs_cleaning_policy {
	const char *name;
	
	/* Lifecycle */
	int (*init)(struct nilfs_cleaning_policy *policy,
		    struct nilfs_cleanerd *cleanerd);
	void (*destroy)(struct nilfs_cleaning_policy *policy);
	
	/* Per-segment evaluation */
	int (*evaluate_segment)(struct nilfs_cleaning_policy *policy,
				struct nilfs_cleanerd *cleanerd,
				const struct nilfs_sustat *sustat,
				const struct nilfs_suinfo *si,
				uint64_t segnum,
				int64_t now,
				int64_t prottime,
				struct nilfs_segment_candidate *candidate);
	
	/* Sorting */
	int (*compare)(const void *elem1, const void *elem2);
	
	/* Optional: custom selection logic */
	ssize_t (*select)(struct nilfs_cleaning_policy *policy,
			  struct nilfs_cleanerd *cleanerd,
			  struct nilfs_sustat *sustat,
			  uint64_t *segnums,
			  int64_t *prottimep,
			  int64_t *oldestp);
	
	/* Policy-specific state */
	void *policy_data;
};

/* Built-in policies */
extern struct nilfs_cleaning_policy nilfs_policy_timestamp;
extern struct nilfs_cleaning_policy nilfs_policy_cost_benefit;
extern struct nilfs_cleaning_policy nilfs_policy_greedy;
extern struct nilfs_cleaning_policy nilfs_policy_hot_cold;

/* Policy registration */
int nilfs_register_policy(struct nilfs_cleaning_policy *policy);
struct nilfs_cleaning_policy *nilfs_get_policy(const char *name);

#endif /* NILFS_CLEANING_POLICY_H */