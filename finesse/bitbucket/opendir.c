//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

void bitbucket_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	void *userdata = fuse_req_userdata(req);
	bitbucket_user_data_t *BBud = (bitbucket_user_data_t *)userdata;
	bitbucket_inode_t *inode = NULL;
	int status = 0;

	// TODO: should we be doing anything with the flags in fi->flags?

	CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);

	if (FUSE_ROOT_ID == ino) {
		inode = BBud->RootDirectory;
	}
	else {
		inode = BitbucketLookupInodeInTable(BBud->InodeTable, ino);
	}

	if (NULL == inode) {
		status = ENOENT;
	}

	while (NULL != inode){
		if (!S_ISDIR(inode->Attributes.st_mode)) {
			// This inode is not a directory inode
			status = ENOTDIR;
		}

		break;
	}


	if (0 != status) {
		fuse_reply_err(req, status);
	}
	else {
		fi->fh = inode->Attributes.st_mode;
		fi->cache_readdir = 1; // permit caching
		fuse_reply_open(req, fi);
	}

	if (NULL != inode) {
		BitbucketDereferenceInode(inode);
		inode = NULL;
	}

}
