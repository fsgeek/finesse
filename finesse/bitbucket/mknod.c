//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include "bitbucketcalls.h"
#include <errno.h>

static int bitbucket_internal_mknod(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, dev_t rdev);


void bitbucket_mknod(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, dev_t rdev)
{
	struct timespec start, stop, elapsed;
	int status, tstatus;

	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	assert(0 == tstatus);
	status = bitbucket_internal_mknod(req, parent, name, mode, rdev);
	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
	assert(0 == tstatus);
	timespec_diff(&start, &stop, &elapsed);
	bitbucket_count_call(BITBUCKET_CALL_MKNOD, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_mknod(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, dev_t rdev)
{
	(void) req;
	(void) parent;
	(void) mode;
	(void) name;
	(void) rdev;

	assert(0); // Not implemented

	return ENOTSUP;
}
