//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include <errno.h>
#include <string.h>

void bitbucket_write(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t off, struct fuse_file_info *fi)
{
	void *userdata = fuse_req_userdata(req);
	bitbucket_userdata_t *BBud = (bitbucket_userdata_t *)userdata;
	bitbucket_inode_t *inode = NULL;
	int status = 0;
	int extending = 0;

	(void) buf;
	(void) fi;

	if (0 == size) {
		// zero byte writes always succeed
		fuse_reply_write(req, 0);
		return;
	}

	// TODO: should we be doing anything with the flags in fi->flags?

	CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);

	if (FUSE_ROOT_ID == ino) {
		inode = BBud->RootDirectory;
		BitbucketReferenceInode(inode, INODE_LOOKUP_REFERENCE); // must have a lookup ref.
	}
	else {
		inode = BitbucketLookupInodeInTable(BBud->InodeTable, ino); // returns a lookup ref.	
	}

	//
	// We now support storage!
	//
	status = EBADF;
	
	while (NULL != inode) {

		if (BITBUCKET_FILE_TYPE != inode->InodeType) {
			status = EINVAL;
			break;			
		}

		status = 0; // success

		BitbucketLockInode(inode, 0); // prevent size changes
		if (off + size > inode->Attributes.st_size) {
			extending = 1; // can't do this with the read lock

			// move the EOF pointer out.
			inode->Attributes.st_size = off + size;
			inode->Attributes.st_blocks = inode->Attributes.st_size / inode->Attributes.st_blksize;

		}
		else {
			char *ptr = (char *)((uintptr_t)inode->Instance.File.Map) + off;
			memcpy(ptr, buf, size);
		}
		BitbucketUnlockInode(inode);

		if (1 == extending) {
			BitbucketLockInode(inode, 1); // we get to change this
			status = BitbucketAdjustFileStorage(inode, off + size);
			BitbucketUnlockInode(inode);
			assert(0 == status); // if not, may be an OK failure, or it may be a code bug...
		}

		BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE);
		inode = NULL;
		break;
	}

	if (0 == status) {
		fuse_reply_write(req, size);
	}
	else {
		fuse_reply_err(req, status);
	}

}
