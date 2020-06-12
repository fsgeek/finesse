//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include <errno.h>

void bitbucket_forget(fuse_req_t req, fuse_ino_t ino, uint64_t nlookup)
{
	void *userdata = fuse_req_userdata(req);
	bitbucket_user_data_t *BBud = (bitbucket_user_data_t *)userdata;
	bitbucket_inode_t *inode = NULL;

	inode = BitbucketLookupInodeInTable(BBud->InodeTable, ino);

	if (NULL == inode) {
		fuse_reply_err(req, EBADF);
		return;
	}

	for (uint64_t index = 0; index < nlookup; index++) {
		BitbucketDereferenceInode(inode, INODE_FUSE_LOOKUP_REFERENCE);
	}

	BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE);

	fuse_reply_err(req, 0);

	return;
}
