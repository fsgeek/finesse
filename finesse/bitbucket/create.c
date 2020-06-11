//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include <errno.h>
#include <string.h>

#if 0
/**
 * Reply with a directory entry and open parameters
 *
 * currently the following members of 'fi' are used:
 *   fh, direct_io, keep_cache
 *
 * Possible requests:
 *   create
 *
 * Side effects:
 *   increments the lookup count on success
 *
 * @param req request handle
 * @param e the entry parameters
 * @param fi file information
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_create(fuse_req_t req, const struct fuse_entry_param *e,
					  const struct fuse_file_info *fi);
#endif // 0


void bitbucket_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi)
{
	void *userdata = fuse_req_userdata(req);
	bitbucket_user_data_t *BBud = (bitbucket_user_data_t *)userdata;
	bitbucket_inode_t *parentInode = NULL;
	bitbucket_inode_t *inode = NULL;
	struct fuse_entry_param fep;
	int status = EEXIST;
	mode_t mask;

	(void)req;
	(void)parent;
	(void)name;
	(void)mode;
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
				inode = BitbucketCreateFile(parentInode, name);
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

		fuse_reply_create(req, &fep, fi);
		status = 0;
		break;
	}

	if (0 != status) {
		fuse_reply_err(req, status);
	}

	if (NULL != inode) {
		BitbucketReferenceInode(inode, INODE_FUSE_REFERENCE);
		BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE);
	}

}
