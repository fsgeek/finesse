//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include <errno.h>

void bitbucket_getxattr(fuse_req_t req, fuse_ino_t ino, const char *name, size_t size)
{
	void *userdata = fuse_req_userdata(req);
	bitbucket_user_data_t *BBud = (bitbucket_user_data_t *)userdata;
	bitbucket_inode_t *inode = NULL;
	size_t length;
	const void *data;
	int status = EBADF;

	CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);

	if (FUSE_ROOT_ID == ino) {
		inode = BBud->RootDirectory;
		BitbucketReferenceInode(inode, INODE_LOOKUP_REFERENCE);
	}
	else {
		inode = BitbucketLookupInodeInTable(BBud->InodeTable, ino);
	}

	while (NULL != inode) {

		BitbucketLockInode(inode, 0);
		status = BitbucketLookupExtendedAttribute(inode, name, &length, &data);

		if (0 != status) {
			fuse_reply_err(req, status);
			break; // ENOENT probably
		}

		if (size == 0) {
			fuse_reply_xattr(req, size);
			status = 0;
			break;
		}

		if (size < length) {
			status = ERANGE;
			break;
		}

		fuse_reply_buf(req, data, length);
		status = 0;
		break;
	}

	if (NULL != inode) {
		BitbucketUnlockInode(inode);
		BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE);
		inode = NULL;
	}

}
