//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include <errno.h>
#include <string.h>

void bitbucket_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi)
{
	void *userdata = fuse_req_userdata(req);
	bitbucket_userdata_t *BBud = (bitbucket_userdata_t *)userdata;
	bitbucket_inode_t *parentInode = NULL;
	bitbucket_inode_t *inode = NULL;
	struct fuse_entry_param fep;
	int status = EEXIST;
	mode_t mask;

	(void)fi;

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

	while (NULL == inode) {
		mask = mode;
		mask &= S_IFMT;

		switch(mask) {
			case S_IFDIR: {
				inode = BitbucketCreateDirectory(parentInode, name);
			}
			break;
			case S_IFCHR: {
				assert(0); // To be implemented - create a character device
			}
			break;
			case S_IFBLK: {
				assert(0); // To be implemented - create a block device
			}
			break;
			case 0: // default to a regular file if nothing specified
			case S_IFREG: {
				inode = BitbucketCreateFile(parentInode, name, BBud);
			}
			break;
			case S_IFIFO: {
				assert(0); // create fifo = to be implemented
			}
			break;
			case S_IFLNK: {
				assert(0); // create symlink - to be implemented
			}
			break;
			case S_IFSOCK: {
				assert(0); // create socket - to be implemented
			}
			break;
			default: {
				status = EINVAL;
			}
			break;
				
		}

		if (NULL == inode) {
			// something failed, we're done
			break;
		}

		// this preserves suid and sgid - is it the right policy?
		inode->Attributes.st_mode = mask | (mode & ALLPERMS);

		memset(&fep, 0, sizeof(fep));
		fep.ino = inode->Attributes.st_ino;
		fep.generation = inode->Epoch;
		fep.attr = inode->Attributes;
		fep.attr_timeout = 30;
		fep.entry_timeout = 30;

		status = 0;
		break;
	}

	if (0 != status) {
		fuse_reply_err(req, status);
	}
	else {
		assert(NULL != inode);

		// Note: this isn't well-documented.  FUSE seems to treat
		// create as both a lookup and an open.
		// (test this by creating a file and then immediately deleting it).
		fi->fh = (uint64_t)inode;
		BitbucketReferenceInode(inode, INODE_FUSE_LOOKUP_REFERENCE);
		BitbucketReferenceInode(inode, INODE_FUSE_OPEN_REFERENCE);
		BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE);
		inode = NULL;
		
		fuse_reply_create(req, &fep, fi);
	}

	assert(NULL == inode);

	if (NULL != parentInode) {
		BitbucketDereferenceInode(parentInode, INODE_LOOKUP_REFERENCE);
	}

}
