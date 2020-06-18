//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include <errno.h>

void bitbucket_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	void *userdata = fuse_req_userdata(req);
	bitbucket_userdata_t *BBud = (bitbucket_userdata_t *)userdata;
	bitbucket_inode_t *inode = NULL;
	bitbucket_inode_t *child = NULL;
	list_entry_t *le = NULL;
	unsigned count = 0;
	int status = EBADF;

	CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);

	if (FUSE_ROOT_ID == parent) {
		inode = BBud->RootDirectory;
		BitbucketReferenceInode(inode, INODE_LOOKUP_REFERENCE);
	}
	else {
		inode = BitbucketLookupInodeInTable(BBud->InodeTable, parent);
	}

	while (NULL != inode) {
		if (BITBUCKET_DIR_TYPE != inode->InodeType) {
			status = ENOTDIR;
			break;
		}

		BitbucketLookupObjectInDirectory(inode, name, &child);
		if (NULL == child) {
			status = ENOENT;
			break;
		}

		if (BITBUCKET_DIR_TYPE != child->InodeType) {
			status = ENOTDIR;
			break;
		}

		// This really should be in dir.c...
		BitbucketLockInode(child, 0);
		count = 0;
		list_for_each(&child->Instance.Directory.Entries, le) {
			count++;
		}
		BitbucketUnlockInode(child);

		if (count > 2) {
			// definitely not empty
			status = ENOTEMPTY;
			break;
		}

		status = BitbucketDeleteDirectoryEntry(inode, name);

		if (0 != status) {
			break;
		}

		// Note: there is a real race condition here, since we
		// are doing this without locks.  IF someone were to add
		// a file to this directory WHILE WE ARE DELETING IT,
		// this will fail.
		//
		// The race is narrow enough that I'm ignoring it for
		// now, but if you see this assert fire, that's probably
		// what is going on.  Fixing it will require locking
		// around both directories; simplest is a "directory
		// deletion" lock that is acquired (shared) in the
		// direntry creation path..
		//
		// Directory deletions are fairly rare events.  This
		// CAN happen (I've seen it before).
		//
		status = BitbucketDeleteDirectory(child);
		assert(0 == status);
		BitbucketDereferenceInode(child, INODE_LOOKUP_REFERENCE);
		child = NULL;
		break;

	}

	fuse_reply_err(req, status);

	if (NULL != inode) {
		BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE);
	}

}
