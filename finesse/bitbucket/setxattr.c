//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include "bitbucketcalls.h"

static int bitbucket_internal_setxattr(fuse_req_t req, fuse_ino_t ino, const char *name, const char *value, size_t size, int flags);


void bitbucket_setxattr(fuse_req_t req, fuse_ino_t ino, const char *name, const char *value, size_t size, int flags)
{
	struct timespec start, stop, elapsed;
	int status, tstatus;

	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	assert(0 == tstatus);
	status = bitbucket_internal_setxattr(req, ino, name, value, size, flags);
	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
	assert(0 == tstatus);
	timespec_diff(&start, &stop, &elapsed);
	BitbucketCountCall(BITBUCKET_CALL_SETXATTR, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_setxattr(fuse_req_t req, fuse_ino_t ino, const char *name, const char *value, size_t size, int flags)
{
	(void) req;
	(void) ino;
	(void) name;
	(void) value;
	(void) size;
	(void) flags;

	assert(0); // Not implemented
}
