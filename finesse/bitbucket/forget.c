//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include "bitbucketcalls.h"
#include <errno.h>

static int bitbucket_internal_forget(fuse_req_t req, fuse_ino_t ino, uint64_t nlookup);


void bitbucket_forget(fuse_req_t req, fuse_ino_t ino, uint64_t nlookup)
{
	struct timespec start, stop, elapsed;
	int status, tstatus;

	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	assert(0 == tstatus);
	status = bitbucket_internal_forget(req, ino, nlookup);
	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
	assert(0 == tstatus);
	timespec_diff(&start, &stop, &elapsed);
	bitbucket_count_call(BITBUCKET_CALL_FORGET, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_forget(fuse_req_t req, fuse_ino_t ino, uint64_t nlookup)
{
	void *userdata = fuse_req_userdata(req);
	bitbucket_userdata_t *BBud = (bitbucket_userdata_t *)userdata;
	bitbucket_inode_t *inode = NULL;

	inode = BitbucketLookupInodeInTable(BBud->InodeTable, ino);

	if (NULL == inode) {
		fuse_reply_err(req, EBADF);
		return EBADF;
	}

	for (uint64_t index = 0; index < nlookup; index++) {
		BitbucketDereferenceInode(inode, INODE_FUSE_LOOKUP_REFERENCE);
	}

	BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE);

	fuse_reply_err(req, 0);

	return 0;
}
