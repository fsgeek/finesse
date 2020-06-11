//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"

void bitbucket_init(void *userdata, struct fuse_conn_info *conn)
{
	bitbucket_user_data_t *BBud = (bitbucket_user_data_t *)userdata;
	bitbucket_inode_t *bbinode = NULL;

	assert(NULL != conn);

	// Options we could set:
	// FUSE_CAP_EXPORT_SUPPORT - allow lookup of . and ..
	// FUSE_CAP_SPLICE_MOVE - allows grabbing pages
	// FUSE_CAP_AUTO_INVAL_DATA - this is something we should probably set in Finesse mode
	// FUSE_CAP_WRITEBACK_CACHE - enable writeback data caching
	// FUSE_CAP_POSIX_ACL - this would entail adding support for POSIX ACLs
	// unsigned time_gran; -- we internally have nanosecond granularity (default) but
	// we aren't populating the structures with it currently (should fix)
	//
	conn->want |= FUSE_CAP_WRITEBACK_CACHE;
	conn->want |= FUSE_CAP_EXPORT_SUPPORT;

	BBud->Magic = BITBUCKET_USER_DATA_MAGIC;
	CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);
	BBud->InodeTable = BitbucketCreateInodeTable(BITBUCKET_INODE_TABLE_BUCKETS);
	assert(NULL != BBud->InodeTable);
	BBud->RootDirectory = BitbucketCreateRootDirectory(BBud->InodeTable);
	assert(NULL != BBud->RootDirectory);

	bbinode = BitbucketCreateDirectory(BBud->RootDirectory, "Bitbucket");
	assert(NULL != bbinode);

	// Note: at this point we own one (LOOKUP) reference on this inode.
	
}
