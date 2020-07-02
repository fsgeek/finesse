//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include "bitbucketcalls.h"
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static int bitbucket_internal_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode);


void bitbucket_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode)
{
	struct timespec start, stop, elapsed;
	int status, tstatus;

	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	assert(0 == tstatus);
	status = bitbucket_internal_mkdir(req, parent, name, mode);
	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
	assert(0 == tstatus);
	timespec_diff(&start, &stop, &elapsed);
	BitbucketCountCall(BITBUCKET_CALL_MKDIR, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode)
{
	void *userdata = fuse_req_userdata(req);
	bitbucket_userdata_t *BBud = (bitbucket_userdata_t *)userdata;
	bitbucket_inode_t *inode = NULL;
	bitbucket_inode_t *child = NULL;
	int status = 0;
	struct fuse_entry_param fep;


	// TODO: should we be doing anything with the flags in fi->flags?

	CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);

	if (FUSE_ROOT_ID == parent) {
		inode = BBud->RootDirectory;
		BitbucketReferenceInode(inode, INODE_LOOKUP_REFERENCE); // must have a lookup ref.
	}
	else {
		inode = BitbucketLookupInodeInTable(BBud->InodeTable, parent); // returns a lookup ref.	
	}

	if (NULL == inode) {
		status = ENOENT;
	}

	while (NULL != inode){
		if (!S_ISDIR(inode->Attributes.st_mode)) {
			// This inode is not a directory inode
			status = ENOTDIR;
			break;
		}

		child = BitbucketCreateDirectory(inode, name);

		if (NULL == child) {
			status = EEXIST;
			break;
		}

		// keep existing file type bits, use the passed-in mode bits.
		child->Attributes.st_mode = (child->Attributes.st_mode & S_IFMT) | (mode  & ~S_IFMT);

		memset(&fep, 0, sizeof(fep));
		fep.ino = child->Attributes.st_ino;
		fep.generation = child->Epoch;
		fep.attr = child->Attributes;
		fep.attr_timeout = BBud->AttrTimeout;
		fep.entry_timeout = BBud->AttrTimeout;

		status = 0;
		break;
	}

	if (0 != status) {
		fuse_reply_err(req, status);
	}
	else {
		assert(NULL != child);
		BitbucketReferenceInode(child, INODE_FUSE_LOOKUP_REFERENCE); // matches this fuse_reply_entry
		fuse_reply_entry(req, &fep);
	}

	if (NULL != inode) {
		BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE, 1);
		inode = NULL;
	}

	if (NULL != child) {
		BitbucketDereferenceInode(child, INODE_LOOKUP_REFERENCE, 1);
		child = NULL;
	}

	return status;
}
