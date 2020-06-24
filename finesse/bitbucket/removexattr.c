//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include "bitbucketcalls.h"
#include <errno.h>

static int bitbucket_internal_removexattr(fuse_req_t req, fuse_ino_t ino, const char *name);


void bitbucket_removexattr(fuse_req_t req, fuse_ino_t ino, const char *name)
{
	struct timespec start, stop, elapsed;
	int status, tstatus;

	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	assert(0 == tstatus);
	status = bitbucket_internal_removexattr(req, ino, name);
	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
	assert(0 == tstatus);
	timespec_diff(&start, &stop, &elapsed);
	BitbucketCountCall(BITBUCKET_CALL_REMOVEXATTR, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_removexattr(fuse_req_t req, fuse_ino_t ino, const char *name)
{
	(void) req;
	(void) ino;
	(void) name;
	
	assert(0); // Not implemented

	return ENOTSUP;
}
