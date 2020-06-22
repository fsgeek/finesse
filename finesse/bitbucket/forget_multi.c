//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include "bitbucketcalls.h"

static int bitbucket_internal_forget_multi(fuse_req_t req, size_t count, struct fuse_forget_data *forgets);


void bitbucket_forget_multi(fuse_req_t req, size_t count, struct fuse_forget_data *forgets)
{
	struct timespec start, stop, elapsed;
	int status, tstatus;

	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	assert(0 == tstatus);
	status = bitbucket_internal_forget_multi(req, count, forgets);
	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
	assert(0 == tstatus);
	timespec_diff(&start, &stop, &elapsed);
	bitbucket_count_call(BITBUCKET_CALL_FORGET_MULTI, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_forget_multi(fuse_req_t req, size_t count, struct fuse_forget_data *forgets)
{
	void *userdata = fuse_req_userdata(req);
	bitbucket_userdata_t *BBud = (bitbucket_userdata_t *)userdata;
	bitbucket_inode_t *inode = NULL;
	struct fuse_forget_data *forget_array = forgets;


	for (unsigned index = 0; index < count; index++) {

		inode = BitbucketLookupInodeInTable(BBud->InodeTable, forget_array[index].ino);

		if (NULL != inode) {
			for (uint64_t decrement = 0; decrement < forget_array[index].nlookup; decrement++) {
				BitbucketDereferenceInode(inode, INODE_FUSE_LOOKUP_REFERENCE);
			}

			BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE); // this is from **our** lookup
			inode = NULL;
		}

	}

	fuse_reply_err(req, 0);

	return 0;

}
