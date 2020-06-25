//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include "bitbucketcalls.h"
#include <errno.h>

static int bitbucket_internal_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);


void bitbucket_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	struct timespec start, stop, elapsed;
	int status, tstatus;

	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	assert(0 == tstatus);
	status = bitbucket_internal_getattr(req, ino, fi);
	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
	assert(0 == tstatus);
	timespec_diff(&start, &stop, &elapsed);
	BitbucketCountCall(BITBUCKET_CALL_GETATTR, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	void *userdata = fuse_req_userdata(req);
	bitbucket_userdata_t *BBud = (bitbucket_userdata_t *)userdata;
	bitbucket_inode_t *inode = NULL;

	if (NULL != fi) {
		// adding this to see if I can use the "state" safely...
		inode = (bitbucket_inode_t *)fi->fh;
		CHECK_BITBUCKET_INODE_MAGIC(inode);
		assert(ino == inode->Attributes.st_ino);
	}

	(void) fi; // TODO: should we do something with the Fi?

	CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);

	if (FUSE_ROOT_ID == ino) {
		inode = BBud->RootDirectory;
		BitbucketReferenceInode(inode, INODE_LOOKUP_REFERENCE);
	}
	else {
		inode = BitbucketLookupInodeInTable(BBud->InodeTable, ino);
	}

	if (NULL == inode) {
		fuse_reply_err(req, EBADF);
		return EBADF;
	}

	fuse_reply_attr(req, &inode->Attributes, BBud->AttrTimeout);

	BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE, 1);
	inode = NULL;

	return 0;
}
