//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"

void bitbucket_init(void *userdata, struct fuse_conn_info *conn)
{
	bitbucket_user_data_t *BBud = (bitbucket_user_data_t *)userdata;
	(void) conn;

	BBud->Magic = BITBUCKET_USER_DATA_MAGIC;
	CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);
	BBud->InodeTable = BitbucketCreateInodeTable(BITBUCKET_INODE_TABLE_BUCKETS);
	assert(NULL != BBud->InodeTable);
	BBud->RootDirectory = BitbucketCreateRootDirectory(BBud->InodeTable);
	assert(NULL != BBud->RootDirectory);

	// Note: at this point we own one (LOOKUP) reference on this inode.
	
}
