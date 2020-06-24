//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include "bitbucketcalls.h"
#include <errno.h>
#include <sys/mman.h>

static int bitbucket_internal_fsync(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi);


void bitbucket_fsync(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi)
{
	struct timespec start, stop, elapsed;
	int status, tstatus;

	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	assert(0 == tstatus);
	status = bitbucket_internal_fsync(req, ino, datasync, fi);
	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
	assert(0 == tstatus);
	timespec_diff(&start, &stop, &elapsed);
	BitbucketCountCall(BITBUCKET_CALL_FSYNC, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_fsync(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi)
{
	void *userdata = fuse_req_userdata(req);
	bitbucket_userdata_t *BBud = (bitbucket_userdata_t *)userdata;
	bitbucket_inode_t *inode = NULL;
	int status = EBADF;

	(void) datasync;
	(void) fi;

	CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);

	if (FUSE_ROOT_ID == ino) {
		inode = BBud->RootDirectory;
		BitbucketReferenceInode(inode, INODE_LOOKUP_REFERENCE);
	}
	else {
		inode = BitbucketLookupInodeInTable(BBud->InodeTable, ino);
	}

	while (NULL != inode) {
		status = 0;

		if (BITBUCKET_FILE_TYPE == inode->InodeType) {
			BitbucketLockInode(inode, 0);
			if (NULL != inode->Instance.File.Map) {
				status = msync(inode->Instance.File.Map, inode->Attributes.st_size, MS_SYNC);
				assert(0 == status);
			}
			BitbucketUnlockInode(inode);
		}

		break;
	}

	if (NULL == inode) {
		status = EBADF;
	}

	fuse_reply_err(req, status);

	if (NULL != inode) {
		BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE);
		inode = NULL;
	}

	return status;
	
}
