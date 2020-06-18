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
	bitbucket_userdata_t *BBud = (bitbucket_userdata_t *)userdata;
	bitbucket_inode_t *parentInode = NULL;
	bitbucket_inode_t *inode = NULL;
	struct fuse_entry_param fep;

	CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);

	if (FUSE_ROOT_ID == parent) {
		parentInode = BBud->RootDirectory;
		BitbucketReferenceInode(parentInode, INODE_LOOKUP_REFERENCE);
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

	memset(&fep, 0, sizeof(fep));
	fep.ino = inode->Attributes.st_ino;
	fep.generation = inode->Epoch;
	fep.attr = inode->Attributes;
	fep.attr_timeout = 30;
	fep.entry_timeout = 30;

	BitbucketReferenceInode(inode, INODE_FUSE_LOOKUP_REFERENCE); // this matches the fuse_reply_entry
	fuse_reply_entry(req, &fep);

	if (BBud->Debug) {
		fuse_log(FUSE_LOG_DEBUG, "  %lli/%s -> %lli\n",
			(unsigned long long) parent, name, (unsigned long long) inode->Attributes.st_ino);
	}

	if (NULL != parentInode) {
		BitbucketDereferenceInode(parentInode, INODE_LOOKUP_REFERENCE);
	}

}
