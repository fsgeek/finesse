//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include "bitbucketcalls.h"
#include <fuse_lowlevel.h>
#include <errno.h>
#include <linux/fs.h> // flags
#include <string.h>

static int bitbucket_internal_rename(fuse_req_t req, fuse_ino_t parent, const char *name, fuse_ino_t newparent, const char *newname, unsigned int flags);


void bitbucket_rename(fuse_req_t req, fuse_ino_t parent, const char *name, fuse_ino_t newparent, const char *newname, unsigned int flags)
{
	struct timespec start, stop, elapsed;
	int status, tstatus;

	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	assert(0 == tstatus);
	status = bitbucket_internal_rename(req, parent, name, newparent, newname, flags);
	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
	assert(0 == tstatus);
	timespec_diff(&start, &stop, &elapsed);
	BitbucketCountCall(BITBUCKET_CALL_RENAME, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_rename(fuse_req_t req, fuse_ino_t parent, const char *name, fuse_ino_t newparent, const char *newname, unsigned int flags)
{
	bitbucket_userdata_t *BBud = (bitbucket_userdata_t *)fuse_req_userdata(req);
	bitbucket_inode_t *old_parent = NULL;
	bitbucket_inode_t *new_parent = NULL;
	bitbucket_inode_t *old_inode = NULL;
	bitbucket_inode_t *new_inode = NULL;
	int status = EBADF;

	// For a persistent file system, rename is one of the more challenging calls atomically.  For an in-memory file system
	// we're (much) less concerned.
	//
	// Semantics here are somewhat complicated:
	//
	// *flags* may be `RENAME_EXCHANGE` or `RENAME_NOREPLACE`. If
	// * RENAME_NOREPLACE is specified, the filesystem must not
	// * overwrite *newname* if it exists and return an error
	// * instead. If `RENAME_EXCHANGE` is specified, the filesystem
	// * must atomically exchange the two files, i.e. both must
	// * exist and neither may be deleted.
	//

	assert(NULL != BBud);
	CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);

	// We don't permit renaming . or .. (as per the rename documentation)
	if ((0 == strcmp(name, ".")) || (0 == strcmp(name, "..")) || (0 == strcmp(newname, ".")) || (0 == strcmp(newname, ".."))) {
		status = EINVAL;
		return status;
	}

	if (FUSE_ROOT_ID == parent) {
		old_parent = BBud->RootDirectory;
		BitbucketReferenceInode(old_parent, INODE_LOOKUP_REFERENCE); // must have a lookup ref.
	}
	else {
		old_parent = BitbucketLookupInodeInTable(BBud->InodeTable, parent); // returns a lookup ref.	
	}

	if (FUSE_ROOT_ID == newparent) {
		new_parent = BBud->RootDirectory;
		BitbucketReferenceInode(new_parent, INODE_LOOKUP_REFERENCE); // must have a lookup ref.
	}
	else {
		new_parent = BitbucketLookupInodeInTable(BBud->InodeTable, newparent);
	}

	while ((NULL != old_parent) && (NULL != new_parent)) {

		if ((BITBUCKET_DIR_TYPE != old_parent->InodeType) || (BITBUCKET_DIR_TYPE != new_parent->InodeType)) {
			// we rename from one directory to another
			status = ENOTDIR;
			break;
		}

		BitbucketLookupObjectInDirectory(old_parent, name, &old_inode);

		if (NULL == old_inode) {
			status = ENOENT;
			break;
		}

		if (old_inode == BBud->RootDirectory) {
			// invalid - don't rename the root directory
			status = EINVAL;
			break;
		}

		BitbucketLookupObjectInDirectory(new_parent, newname, &new_inode);

		if (NULL != new_inode) {

			if (new_inode == BBud->RootDirectory) {
				// invalid - don't rename TO the root directory
				status = EINVAL;
				break;
			}

			if (flags & RENAME_NOREPLACE) {
				status = EEXIST;
				break;
			}

			if (flags & RENAME_EXCHANGE) {
				status = BitbucketExchangeObjectsInDirectory(old_parent, new_parent, name, newname);
				break;
			}

			if (BITBUCKET_DIR_TYPE == new_inode->InodeType) {

				if (BitbucketDirectoryEntryCount(new_inode) > 1) {
					status = ENOTEMPTY;
					break;
				}
			}

			// Let's try to delete it
			status = BitbucketDeleteDirectoryEntry(new_parent, newname);
			if (0 != status) {
				// Deletion failed, so we're going to bail out.
				break;
			}

			// This means it is now officially gone, though note we still have it referenced, so
			// we CAN recover from an error state.

		}

		// We try to insert the existing inode into the new directory
		status = BitbucketInsertDirectoryEntry(new_parent, old_inode, newname);
		if (0 != status) {
			// So this didn't work.  If we still have a new_inode it means we
			// should try to insert it back into the directory.
			int status2 = BitbucketInsertDirectoryEntry(new_parent, new_inode, newname);

			assert(0 == status2); // Not handling this case

			break;
		}

		// If we make it to this point, we need to remove the old entry.
		status = BitbucketDeleteDirectoryEntry(old_parent, name);
		break;

	}

	fuse_reply_err(req, status);

	if (NULL != old_parent) {
		BitbucketDereferenceInode(old_parent, INODE_LOOKUP_REFERENCE, 1);
		old_parent = NULL;
	}

	if (NULL != new_parent) {
		BitbucketDereferenceInode(new_parent, INODE_LOOKUP_REFERENCE, 1);
		new_parent = NULL;
	}

	if (NULL != old_inode) {
		BitbucketDereferenceInode(old_inode, INODE_LOOKUP_REFERENCE, 1);
		old_inode = NULL;
	}

	if (NULL != new_inode) {
		BitbucketDereferenceInode(new_inode, INODE_LOOKUP_REFERENCE, 1);
		new_inode = NULL;
	}


	return status;
}
