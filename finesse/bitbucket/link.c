//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include <errno.h>
#include <string.h>

void bitbucket_link(fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent, const char *newname)
{
	bitbucket_userdata_t *BBud = (bitbucket_userdata_t *)fuse_req_userdata(req);
	bitbucket_inode_t *inode = NULL;
	bitbucket_inode_t *parent = NULL;
	int status = EBADF;
	struct fuse_entry_param fep;

	assert(NULL != BBud);
	CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);

	if (FUSE_ROOT_ID == ino) {
		// We don't support links on directories
		fuse_reply_err(req, EISDIR);
		return;
	}
	
	inode = BitbucketLookupInodeInTable(BBud->InodeTable, ino);

	while (NULL != inode) {

		if (FUSE_ROOT_ID == newparent) {
			parent = BBud->RootDirectory;
			BitbucketReferenceInode(parent, INODE_LOOKUP_REFERENCE);
		}
		else {
			parent = BitbucketLookupInodeInTable(BBud->InodeTable, newparent);
		}
		if (NULL == parent) {
			status = EBADF;
			break;
		}

		if (BITBUCKET_FILE_TYPE != inode->InodeType) {
			if (BITBUCKET_DIR_TYPE == inode->InodeType) {
				status = EISDIR;
			}
			else {
				status = EINVAL;
			}
			break;
		}

		// We need to deal with the case where the link _target_
		// already exists; for now, we delete the target.
		status = EEXIST;
		while (EEXIST == status) {
			status = BitbucketInsertDirectoryEntry(parent, inode, newname);

			if (EEXIST == status) {
				// This is the complex path, where we have to remove an entry
				status = BitbucketRemoveFileFromDirectory(parent, newname);
			}
		}

		memset(&fep, 0, sizeof(fep));
		fep.ino = inode->Attributes.st_ino;
		fep.generation = inode->Epoch;
		fep.attr = inode->Attributes;
		fep.attr_timeout = BBud->AttrTimeout;
		fep.entry_timeout = BBud->AttrTimeout;

		BitbucketReferenceInode(inode, INODE_FUSE_LOOKUP_REFERENCE);

		fuse_reply_entry(req, &fep);

		// At this point, we're done.
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

	if (NULL != parent) {
		BitbucketDereferenceInode(parent, INODE_LOOKUP_REFERENCE);
		parent = NULL;
	}
}
