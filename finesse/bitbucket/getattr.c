//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include <errno.h>

void bitbucket_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	void *userdata = fuse_req_userdata(req);
	bitbucket_user_data_t *BBud = (bitbucket_user_data_t *)userdata;
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
		return;
	}

	fuse_reply_attr(req, &inode->Attributes, BBud->AttrTimeout);

	BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE);
	inode = NULL;

}
