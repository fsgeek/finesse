//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include "bitbucketcalls.h"
#include <errno.h>
#include <string.h>

static int bitbucket_internal_readlink(fuse_req_t req, fuse_ino_t ino);


void bitbucket_readlink(fuse_req_t req, fuse_ino_t ino)
{
	struct timespec start, stop, elapsed;
	int status, tstatus;

	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	assert(0 == tstatus);
	status = bitbucket_internal_readlink(req, ino);
	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
	assert(0 == tstatus);
	timespec_diff(&start, &stop, &elapsed);
	BitbucketCountCall(BITBUCKET_CALL_READLINK, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_readlink(fuse_req_t req, fuse_ino_t ino)
{
	bitbucket_inode_t *inode = NULL;
	bitbucket_userdata_t *bbud = NULL;
	int status = EBADF;

	assert(NULL != req);
	assert(0 != ino);

	bbud = (bitbucket_userdata_t *)fuse_req_userdata(req);
	assert(NULL != bbud);
	CHECK_BITBUCKET_USER_DATA_MAGIC(bbud);

	while (NULL != bbud) {
		inode = BitbucketLookupInodeInTable(bbud->InodeTable, ino);
		if (NULL == inode) {
			status = EBADF;
			break;
		}

		if (BITBUCKET_SYMLINK_TYPE != inode->InodeType) {
			status = EBADF;
			break;
		}

		fuse_reply_buf(req, inode->Instance.SymbolicLink.LinkContents, strlen(inode->Instance.SymbolicLink.LinkContents));
		status = 0;
		break;

	}

	if (0 != status) {
		fuse_reply_err(req, status);
	}

	if (NULL != inode) {
		BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE);
		inode = NULL;
	}

	return status;

}
