//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include <errno.h>
#include <sys/mman.h>

void bitbucket_flush(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	void *userdata = fuse_req_userdata(req);
	bitbucket_userdata_t *BBud = (bitbucket_userdata_t *)userdata;
	bitbucket_inode_t *inode = NULL;
	int status = EBADF;

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
			if (inode->Instance.File.Map) {
				status = msync(inode->Instance.File.Map, inode->Attributes.st_size, MS_ASYNC);
				assert(0 == status); // otherwise it could be a programming bug
			}
			BitbucketUnlockInode(inode);
		}

		break;
	}

	fuse_reply_err(req, status);

	if (NULL != inode) {
		BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE);
		inode = NULL;
	}

}
