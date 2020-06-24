//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include "bitbucketcalls.h"
#include <errno.h>
#include <string.h>

static int bitbucket_internal_copy_file_range(fuse_req_t req, fuse_ino_t ino_in, off_t off_in, struct fuse_file_info *fi_in, fuse_ino_t ino_out, off_t off_out, struct fuse_file_info *fi_out, size_t len, int flags);


void bitbucket_copy_file_range(fuse_req_t req, fuse_ino_t ino_in, off_t off_in, struct fuse_file_info *fi_in, fuse_ino_t ino_out, off_t off_out, struct fuse_file_info *fi_out, size_t len, int flags)
{
	struct timespec start, stop, elapsed;
	int status, tstatus;

	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	assert(0 == tstatus);
	status = bitbucket_internal_copy_file_range(req, ino_in, off_in, fi_in, ino_out, off_out, fi_out, len, flags);
	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
	assert(0 == tstatus);
	timespec_diff(&start, &stop, &elapsed);
	BitbucketCountCall(BITBUCKET_CALL_COPY_FILE_RANGE, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_copy_file_range(fuse_req_t req, fuse_ino_t ino_in, off_t off_in, struct fuse_file_info *fi_in, fuse_ino_t ino_out, off_t off_out, struct fuse_file_info *fi_out, size_t len, int flags)
{

	bitbucket_userdata_t *BBud;
	bitbucket_inode_t *in_inode = NULL;
	bitbucket_inode_t *out_inode = NULL;
	int status = EBADF;
	int copy_size = 0;
	int exclusive = 0;

	(void) off_in;
	(void) off_out;
	(void) len;
	(void) flags;

	// TODO: could we use these?
	(void) fi_in;
	(void) fi_out;

	BBud = (bitbucket_userdata_t *) fuse_req_userdata(req);
	assert(NULL != BBud);
	CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);

	// We don't support copying data contents between directories
	// so we should never have the root directory.
	if ((FUSE_ROOT_ID == ino_in) || (FUSE_ROOT_ID == ino_out)) {
		fuse_reply_err(req, EISDIR);
		return EISDIR;
	}

	in_inode = BitbucketLookupInodeInTable(BBud->InodeTable, ino_in);
	out_inode = BitbucketLookupInodeInTable(BBud->InodeTable, ino_out);

	BitbucketLockTwoInodes(in_inode, out_inode, 0); // shared lock initially
	while ((NULL != in_inode) && (NULL != out_inode)) {

		if ((BITBUCKET_FILE_TYPE != in_inode->InodeType) || (BITBUCKET_FILE_TYPE != out_inode->InodeType)) {
			// We don't support copying data from anything other than files
			status = EINVAL;
			break;
		}

		// Question: would it be better to open the mappings and copy the data?
		copy_size = len;

		while (copy_size > 0) {

			if (in_inode->Attributes.st_size <= off_in) {
				// Nothing to copy here
				copy_size = 0;
				status = 0;
				break;
			}

			// We can't copy more data than we have available
			if (in_inode->Attributes.st_size < (off_in + copy_size)) {
				copy_size = off_in - in_inode->Attributes.st_size;
			}

			assert(copy_size > 0);

			// Is the output file big enough?
			if ((0 == exclusive) && (out_inode->Attributes.st_size < (off_out + copy_size))) {
				// We need to extend this file, which means we have to upgrade our locks
				// While I could probably lock the input file shared and output exclusive
				// to do this, for now I lock them both exclusive.
				BitbucketUnlockInode(in_inode);
				BitbucketUnlockInode(out_inode);

				BitbucketLockTwoInodes(in_inode, out_inode, 1);
				exclusive = 1;
				copy_size = len; // the input file might have changed size
				continue; // we have to start over now that we've reacquired the locks
			}

			if (out_inode->Attributes.st_size < (off_out + copy_size)) {
				BitbucketAdjustFileStorage(out_inode, off_out + copy_size); // adjust the storage space
			}

			assert(out_inode->Attributes.st_size <= (off_out + copy_size)); // make sure it really did move

			memcpy((void *)(((uintptr_t)out_inode->Instance.File.Map)+ off_out),
				   (void *)(((uintptr_t)in_inode->Instance.File.Map) + off_in),
				   copy_size);

			status = 0;
			break;

		}

		if (0 == copy_size) {
			// Nothing to copy
			status = 0;
			break;
		}
	}
	BitbucketUnlockInode(out_inode);
	BitbucketUnlockInode(in_inode);

	if (0 != status) {
		fuse_reply_err(req, status);
	}
	else {
		fuse_reply_write(req, copy_size);
	}

	if (NULL != in_inode) {
		BitbucketDereferenceInode(in_inode, INODE_LOOKUP_REFERENCE);
		in_inode = NULL;
	}

	if (NULL != out_inode) {
		BitbucketDereferenceInode(out_inode, INODE_LOOKUP_REFERENCE);
		out_inode = NULL;
	}

	return status;
}
