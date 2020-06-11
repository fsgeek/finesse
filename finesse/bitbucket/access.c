//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include <errno.h>

void bitbucket_access(fuse_req_t req, fuse_ino_t ino, int mask)
{
	void *userdata = fuse_req_userdata(req);
	bitbucket_user_data_t *BBud = (bitbucket_user_data_t *)userdata;
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
	}

	fuse_reply_err(req, status);

	if (NULL != inode) {
		BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE);
		inode = NULL;
	}


	assert(0); // Not implemented
}
