//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include <errno.h>

void bitbucket_write(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t off, struct fuse_file_info *fi)
{
	void *userdata = fuse_req_userdata(req);
	bitbucket_user_data_t *BBud = (bitbucket_user_data_t *)userdata;
	bitbucket_inode_t *inode = NULL;
	int status = 0;

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
	// This is what makes the file system a bit bucket: writes are discarded (with success, of course)
	//
	status = EBADF;
	
	while (NULL != inode) {

		if (BITBUCKET_FILE_TYPE != inode->InodeType) {
			status = EINVAL;
			break;			
		}

		status = 0; // success

		if (off + size > inode->Attributes.st_size) {
			// move the EOF pointer out.
			inode->Attributes.st_size = off + size;
		}
		BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE);
		inode = NULL;
	}

	if (0 == status) {
		fuse_reply_write(req, size);
	}
	else {
		fuse_reply_err(req, status);
	}

}
