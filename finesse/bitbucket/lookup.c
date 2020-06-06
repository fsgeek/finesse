//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include <errno.h>
#include <string.h>

void bitbucket_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	void *userdata = fuse_req_userdata(req);
	bitbucket_user_data_t *BBud = (bitbucket_user_data_t *)userdata;
	bitbucket_inode_t *parentInode = NULL;
	bitbucket_inode_t *inode = NULL;

	CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);

	if (FUSE_ROOT_ID == parent) {
		parentInode = BBud->RootDirectory;
	}
	else {
		parentInode = BitbucketLookupInodeInTable(BBud->InodeTable, parent);
	}

	if (NULL == parentInode) {
		fuse_reply_err(req, EBADF);
		return;
	}

	BitbucketLookupObjectInDirectory(parentInode, name, &inode);

	if (NULL == inode) {
		fuse_reply_err(req, ENOENT);
		return;
	}

	fuse_reply_attr(req, &inode->Attributes, BBud->AttrTimeout);

	if (BBud->Debug) {
		fuse_log(FUSE_LOG_DEBUG, "  %lli/%s -> %lli\n",
			(unsigned long long) parent, name, (unsigned long long) inode->Attributes.st_ino);
	}

	if (NULL != inode) {
		BitbucketDereferenceInode(inode);
		inode = NULL;
	}

	if (FUSE_ROOT_ID == parent) {
		BitbucketDereferenceInode(parentInode);
	}

}
