//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include "bitbucketcalls.h"
#include <errno.h>

static int bitbucket_internal_access(fuse_req_t req, fuse_ino_t ino, int mask);


void bitbucket_access(fuse_req_t req, fuse_ino_t ino, int mask)
{
	struct timespec start, stop, elapsed;
	int status, tstatus;

	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	assert(0 == tstatus);
	status = bitbucket_internal_access(req, ino, mask);
	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
	assert(0 == tstatus);
	timespec_diff(&start, &stop, &elapsed);
	BitbucketCountCall(BITBUCKET_CALL_ACCESS, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_access(fuse_req_t req, fuse_ino_t ino, int mask)
{
	void *userdata = fuse_req_userdata(req);
	bitbucket_userdata_t *BBud = (bitbucket_userdata_t *)userdata;
	bitbucket_inode_t *inode = NULL;
	int status = EBADF;

	(void) mask;

	CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);

	if (FUSE_ROOT_ID == ino) {
		inode = BBud->RootDirectory;
		BitbucketReferenceInode(inode, INODE_LOOKUP_REFERENCE);
	}
	else {
		inode = BitbucketLookupInodeInTable(BBud->InodeTable, ino);
	}

	while (NULL != inode) {
		mode_t mode = inode->Attributes.st_mode;

		mode &= ~S_IFMT;
		if (mask != (mask & mode)) {
			status = EACCES;
		}
		status = 0;
		break;
	}

	fuse_reply_err(req, status);

	if (NULL != inode) {
		BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE, 1);
		inode = NULL;
	}

	return status;

}
