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
	BBud->RootDirectory = BitbucketCreateDirectory(NULL, NULL); // create root directory
	assert(NULL != BBud->RootDirectory);
	// Should I swap the inode number here?  I'm going to skip doing it now
	// because FUSE_ROOT_ID can be satisfied by explicitly picking the root
	// directory from BBud...
	BitbucketInsertInodeInTable(BBud->InodeTable, BBud->RootDirectory);

}
