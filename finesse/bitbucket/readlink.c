//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include <errno.h>
#include <string.h>

void bitbucket_readlink(fuse_req_t req, fuse_ino_t ino)
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

}
