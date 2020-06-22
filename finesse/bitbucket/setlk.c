//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include "bitbucketcalls.h"
#include <errno.h>

static int bitbucket_internal_setlk(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, struct flock *lock, int sleep);


void bitbucket_setlk(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, struct flock *lock, int sleep)
{
	struct timespec start, stop, elapsed;
	int status, tstatus;

	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	assert(0 == tstatus);
	status = bitbucket_internal_setlk(req, ino, fi, lock, sleep);
	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
	assert(0 == tstatus);
	timespec_diff(&start, &stop, &elapsed);
	bitbucket_count_call(BITBUCKET_CALL_SETLK, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_setlk(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, struct flock *lock, int sleep)
{
	(void) req;
	(void) ino;
	(void) lock;
	(void) sleep;
	(void) fi;

	assert(0); // Not implemented

	return ENOTSUP;
}
