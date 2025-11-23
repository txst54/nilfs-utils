/*
 * cleanerd.h - NILFS cleaner daemon header
 *
 * Copyright (C) 2007-2012 Nippon Telegraph and Telephone Corporation.
 *
 * Licensed under GPLv2: the complete text of the GNU General Public
 * License can be found in COPYING file of the nilfs-utils package.
 */

#ifndef NILFS_CLEANERD_H
#define NILFS_CLEANERD_H

#include <sys/types.h>
#include <sys/time.h>
#include <mqueue.h>
#include <uuid/uuid.h>
#include "nilfs.h"
#include "cldconfig.h"
#include "nilfs_cleaning_policy.h"

/**
 * struct nilfs_cleanerd - nilfs cleaner daemon
 * @nilfs: nilfs object
 * @cnormap: checkpoint number reverse mapper
 * @config: config structure
 * @conffile: configuration file name
 * @policy: cleaning policy
 * @running: running state
 * @fallback: fallback state
 * @retry_cleaning: retrying reclamation for protected segments
 * @no_timeout: the next timeout will be 0 seconds
 * @shutdown: shutdown flag
 * @ncleansegs: number of segments cleaned per cycle
 * @cleaning_interval: cleaning interval
 * @target: target time for sleeping (monotonic time)
 * @timeout: timeout value for sleeping
 * @min_reclaimable_blocks: min. number of reclaimable blocks
 * @prev_nongc_ctime: previous nongc ctime
 * @recvq: receive queue
 * @recvq_name: receive queue name
 * @sendq: send queue
 * @client_uuid: uuid of the previous message received from a client
 * @pending_cmd: pending client command
 * @jobid: current job id
 * @mm_prev_state: previous status during suspending
 * @mm_nrestpasses: remaining number of passes
 * @mm_nrestsegs: remaining number of segment (1-pass)
 * @mm_ncleansegs: number of segments cleaned per cycle (manual mode)
 * @mm_protection_period: protection period (manual mode)
 * @mm_cleaning_interval: cleaning interval (manual mode)
 * @mm_min_reclaimable_blocks: min. number of reclaimable blocks (manual mode)
 */
struct nilfs_cleanerd {
	struct nilfs *nilfs;
	struct nilfs_cnormap *cnormap;
	struct nilfs_cldconfig config;
	char *conffile;
	struct nilfs_cleaning_policy *policy;
	int running;
	int fallback;
	int retry_cleaning;
	int no_timeout;
	int shutdown;
	long ncleansegs;
	struct timespec cleaning_interval;
	struct timespec target;
	struct timespec timeout;
	unsigned long min_reclaimable_blocks;
	uint64_t prev_nongc_ctime;
	mqd_t recvq;
	char *recvq_name;
	mqd_t sendq;
	uuid_t client_uuid;
	unsigned long jobid;
	int mm_prev_state;
	int mm_nrestpasses;
	long mm_nrestsegs;
	long mm_ncleansegs;
	struct timespec mm_protection_period;
	struct timespec mm_cleaning_interval;
	unsigned long mm_min_reclaimable_blocks;
};

#endif /* NILFS_CLEANERD_H */
