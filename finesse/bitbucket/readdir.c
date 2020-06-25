//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include "bitbucketcalls.h"
#include <errno.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <fuse_kernel.h>
#pragma GCC diagnostic pop
#include <stdlib.h>
#include <string.h>

static int bitbucket_internal_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);


void bitbucket_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi)
{
	struct timespec start, stop, elapsed;
	int status, tstatus;

	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	assert(0 == tstatus);
	status = bitbucket_internal_readdir(req, ino, size, off, fi);
	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
	assert(0 == tstatus);
	timespec_diff(&start, &stop, &elapsed);
	BitbucketCountCall(BITBUCKET_CALL_READDIR, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi)
{
	void *userdata = fuse_req_userdata(req);
	bitbucket_userdata_t *BBud = (bitbucket_userdata_t *)userdata;
	bitbucket_inode_t *inode = NULL;
	bitbucket_dir_enum_context_t dirEnumContext;
	char *responseBuffer = NULL;
	size_t used = 0;
	size_t entrySize = 0;
	int status = 0;
	struct stat entryStat;

	(void) req;
	(void) ino;
	(void) size;
	(void) off;
	(void) fi;

	// TODO: should we be doing anything with the flags in fi->flags?

	if (~0 == off) {
		// return the empty buffer
		fuse_reply_buf(req, NULL, 0);
		return 0;
	}

	CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);

	if (FUSE_ROOT_ID == ino) {
		inode = BBud->RootDirectory;
		BitbucketReferenceInode(inode, INODE_LOOKUP_REFERENCE);
	}
	else {
		inode = BitbucketLookupInodeInTable(BBud->InodeTable, ino);
	}

	if (NULL == inode) {
		status = ENOENT;
	}

	BitbucketLockInode(inode, 0); // lock the directory for enumeration (shared)
	while (NULL != inode){
		const bitbucket_dir_entry_t *dirEntry = NULL;

		if (!S_ISDIR(inode->Attributes.st_mode)) {
			// This inode is not a directory inode
			status = ENOTDIR;
			break;
		}

		responseBuffer = malloc(size);
		if (NULL == responseBuffer) {
			status = ENOMEM;
			break;
		}

		BitbucketInitalizeDirectoryEnumerationContext(&dirEnumContext, inode);

		if (0 != off) {
			status = BitbucketSeekDirectory(&dirEnumContext, off);
			if (0 != status) {
				break;
			}
		}

		// As long as we might be able to fit another entry, we keep trying
		// This loop will terminate if:
		//   * We have no more entries
		//   * We don't have space to add more entries
		//   * The context structure indicates the directory has changed
		//
		while ((used + FUSE_NAME_OFFSET) < size) {

			dirEntry = BitbucketEnumerateDirectory(&dirEnumContext);

			if (NULL == dirEntry) {
				// End of the enumeration
				if (0 == used) {
					// In this case we return an error
					status = ENOENT;
				}
				else {
					// we've already packed in some entries, so we have to return them
					status = 0;
				}
				break;
			}

			if (ERESTART == dirEnumContext.LastError) {
				// Directory changed between calls
				assert(0 == used); // we can't have used any yet.
				status = ERESTART;
				break;
			}

			memcpy(&entryStat, &dirEntry->Inode->Attributes, sizeof(struct stat));

			// Let's try to pack an entry into this buffer
			entrySize = fuse_add_direntry(	req,  // FUSE request
											responseBuffer + used, // buffer into which we are packing entries
											size - used,  // space remaining in the buffer
											dirEntry->Name, // name of this object
											&entryStat, // stat struct
											dirEnumContext.Offset); // directory offset

			if (entrySize > size - used) {
				// This entry does not fit.  We can terminate here.
				// If this is the first entry, this will send a zero
				// length response buffer without an error.  It really
				// seems odd, but that's what appears to be expected.
				break;
			}

			used += entrySize;
		}

		// Done at this point
		break;
	}
	BitbucketUnlockInode(inode); // don't need to keep the directory locked any longer

	if (NULL != inode) {
		// release our reference on the directory
		BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE, 1);
		inode = NULL;
	}
	
	if (0 != status) {
		fuse_reply_err(req, status);
	}
	else {
		assert(NULL != responseBuffer);
		fuse_reply_buf(req, responseBuffer, used);
	}

	// cleanup the response buffer
	free(responseBuffer);
	responseBuffer = NULL;

	return status;
}
