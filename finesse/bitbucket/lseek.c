//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include "bitbucketcalls.h"

static int bitbucket_internal_lseek(fuse_req_t req, fuse_ino_t ino, off_t off, int whence, struct fuse_file_info *fi);


void bitbucket_lseek(fuse_req_t req, fuse_ino_t ino, off_t off, int whence, struct fuse_file_info *fi)
{
	struct timespec start, stop, elapsed;
	int status, tstatus;

	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	assert(0 == tstatus);
	status = bitbucket_internal_lseek(req, ino, off, whence, fi);
	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
	assert(0 == tstatus);
	timespec_diff(&start, &stop, &elapsed);
	bitbucket_count_call(BITBUCKET_CALL_LSEEK, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_lseek(fuse_req_t req, fuse_ino_t ino, off_t off, int whence, struct fuse_file_info *fi)
{

	(void) ino;
	(void) whence;
	(void) fi;

	// This is a no-op for us
	fuse_reply_lseek(req, off);

	return 0;

}
