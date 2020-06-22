//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include "bitbucketcalls.h"
#include <errno.h>

static int bitbucket_internal_retrieve_reply(fuse_req_t req, void *cookie, fuse_ino_t ino, off_t offset, struct fuse_bufvec *bufv);


void bitbucket_retrieve_reply(fuse_req_t req, void *cookie, fuse_ino_t ino, off_t offset, struct fuse_bufvec *bufv)
{
	struct timespec start, stop, elapsed;
	int status, tstatus;

	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	assert(0 == tstatus);
	status = bitbucket_internal_retrieve_reply(req, cookie, ino, offset, bufv);
	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
	assert(0 == tstatus);
	timespec_diff(&start, &stop, &elapsed);
	bitbucket_count_call(BITBUCKET_CALL_RETRIEVE_REPLY, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_retrieve_reply(fuse_req_t req, void *cookie, fuse_ino_t ino, off_t offset, struct fuse_bufvec *bufv)
{
	(void) req;
	(void) ino;
	(void) cookie;
	(void) offset;
	(void) bufv;

	assert(0); // Not implemented

	return ENOTSUP;
}
