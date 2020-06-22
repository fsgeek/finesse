//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include "bitbucketcalls.h"

static int bitbucket_internal_flock(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, int op);


void bitbucket_flock(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, int op)
{
	struct timespec start, stop, elapsed;
	int status, tstatus;

	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	assert(0 == tstatus);
	status = bitbucket_internal_flock(req, ino, fi, op);
	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
	assert(0 == tstatus);
	timespec_diff(&start, &stop, &elapsed);
	bitbucket_count_call(BITBUCKET_CALL_FLOCK, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_flock(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, int op)
{
	(void) req;
	(void) ino;
	(void) fi;
	(void) op;

	assert(0); // Not implemented
}
