//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include <errno.h>

void bitbucket_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr, int to_set, struct fuse_file_info *fi)
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
		status = EINVAL;

		if (to_set & FUSE_SET_ATTR_MODE) {
			mode_t updatedMode = inode->Attributes.st_mode & S_IFMT;

			updatedMode |= attr->st_mode & ~S_IFMT; // don't override the file type bits
			inode->Attributes.st_mode = updatedMode;
			status = 0;
		}

		if (to_set & FUSE_SET_ATTR_UID) {
			inode->Attributes.st_uid = attr->st_uid;
			status = 0;
		}

		if (to_set & FUSE_SET_ATTR_GID) {
			inode->Attributes.st_gid = attr->st_gid;
			status = 0;
		}

		if (to_set & FUSE_SET_ATTR_SIZE) {
			status = 0;
			if (inode->Attributes.st_size != attr->st_size) {
				BitbucketLockInode(inode, 1); // exclusive lock
				// note that this call updates the size in the inode
				status = BitbucketAdjustFileStorage(inode, attr->st_size);
				assert(0 == status); // if not, debug
				BitbucketUnlockInode(inode);
			}
		}

		if (to_set & FUSE_SET_ATTR_ATIME) {
			inode->Attributes.st_atime = attr->st_atime;
			status = 0;
		}

		if (to_set & FUSE_SET_ATTR_MTIME) {
			inode->Attributes.st_mtime = attr->st_mtime;
			status = 0;
		}

		if (to_set & FUSE_SET_ATTR_ATIME_NOW) {
		    status = clock_gettime(CLOCK_TAI, &inode->Attributes.st_atim);
			assert(0 == status);
		}

		if (to_set & FUSE_SET_ATTR_MTIME_NOW) {
		    status = clock_gettime(CLOCK_TAI, &inode->Attributes.st_mtim);
			assert(0 == status);
		}

		if (to_set & FUSE_SET_ATTR_CTIME) {
			inode->Attributes.st_ctime = attr->st_ctime;
			status = 0;
		}

		// sanity checks
		if (BITBUCKET_DIR_TYPE == inode->InodeType) {
			assert(S_ISDIR(inode->Attributes.st_mode));
		}

		if (BITBUCKET_FILE_TYPE == inode->InodeType) {
			assert(S_ISREG(inode->Attributes.st_mode));
		}

		break;

	}

	if (0 == status) {
		fuse_reply_attr(req, &inode->Attributes, 30.0);
	}
	else {
		fuse_reply_err(req, status);
	}

	if (NULL != inode) {
		BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE);
		inode = NULL;
	}
}
