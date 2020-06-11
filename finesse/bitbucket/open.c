//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include <errno.h>
#include <string.h>


void bitbucket_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	void *userdata = fuse_req_userdata(req);
	bitbucket_user_data_t *BBud = (bitbucket_user_data_t *)userdata;
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

		status = 0;
		break;
	}

	if (NULL != inode) {
		BitbucketReferenceInode(inode, INODE_FUSE_REFERENCE);
		BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE);
		inode = NULL;
	}

	if (0 != status) {
		fuse_reply_err(req, status);
	}
	else {
		fuse_reply_open(req, fi);
	}

}

