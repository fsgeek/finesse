//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include "bitbucketcalls.h"
#include <errno.h>
#include <string.h>


static int bitbucket_internal_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);


void bitbucket_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	struct timespec start, stop, elapsed;
	int status, tstatus;

	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	assert(0 == tstatus);
	status = bitbucket_internal_open(req, ino, fi);
	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
	assert(0 == tstatus);
	timespec_diff(&start, &stop, &elapsed);
	BitbucketCountCall(BITBUCKET_CALL_OPEN, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	void *userdata = fuse_req_userdata(req);
	bitbucket_userdata_t *BBud = (bitbucket_userdata_t *)userdata;
	bitbucket_inode_t *inode = NULL;
	int status = EEXIST;

	(void) req;
	(void) ino;
	(void) fi;

	CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);

	if (FUSE_ROOT_ID == ino) {
		inode = BBud->RootDirectory;
		BitbucketReferenceInode(inode, INODE_LOOKUP_REFERENCE);
	}
	else {
		inode = BitbucketLookupInodeInTable(BBud->InodeTable, ino);
	}

	status = EBADF;

	while (NULL != inode) {
		fi->fh = (uint64_t)inode;
		BitbucketReferenceInode(inode, INODE_FUSE_LOOKUP_REFERENCE); // matches forget call

		status = 0;
		if (0 != (fi->flags & O_TRUNC)) {
			BitbucketLockInode(inode, 1);
			if (inode->Attributes.st_size > 0) {
				status = BitbucketAdjustFileStorage(inode, 0);
			}
			BitbucketUnlockInode(inode);
			assert(0 == status);
		}

		break;
	}

	if (NULL != inode) {
		BitbucketReferenceInode(inode, INODE_FUSE_OPEN_REFERENCE);   // matches release call
		BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE, 1);    // drops our lookup ref on entry
		inode = NULL;
	}

	if (0 != status) {
		fuse_reply_err(req, status);
	}
	else {
		fuse_reply_open(req, fi);
	}

	return status;

}

