//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include "string.h"

static const char *BitbucketMagicNames[] = {
	"Bitbucket",
	"Size",
	"Name",
	"Creation",
	"Uuid",
	"Unused10",
	"Unused9",
	"Unused8",
	"Unused7",
	"Unused6",
	"Unused5",
	"Unused4",
	"Unused3",
	"Unused2",
	"Unused1",
	"Unused0",
};

void bitbucket_init(void *userdata, struct fuse_conn_info *conn)
{
	bitbucket_user_data_t *BBud = (bitbucket_user_data_t *)userdata;
	unsigned index = 0;

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

	BBud->BitbucketMagicDirectories[0].Name = BitbucketMagicNames[0];
	BBud->BitbucketMagicDirectories[0].Inode = BitbucketCreateDirectory(BBud->RootDirectory, BitbucketMagicNames[0]);
	assert(NULL != BBud->BitbucketMagicDirectories[0].Inode);
	BBud->BitbucketMagicDirectories[0].Inode->Attributes.st_mode &= ~0222; // strip off write bits

	for(index++; index < sizeof(BitbucketMagicNames)/ sizeof(const char *); index++) {
		if (0 == strncmp("Unused", BitbucketMagicNames[index], 6)) {
			continue; // skip these for now.
		}
		BBud->BitbucketMagicDirectories[index].Name = BitbucketMagicNames[index];
		BBud->BitbucketMagicDirectories[index].Inode = BitbucketCreateDirectory(BBud->BitbucketMagicDirectories[0].Inode,
																			    BBud->BitbucketMagicDirectories[index].Name);
		assert(NULL != BBud->BitbucketMagicDirectories[index].Inode);
		BBud->BitbucketMagicDirectories[index].Inode->Attributes.st_mode &= ~0222; // strip off write bits
	}

	// All of these inodes have a lookup reference on them.
	
}


void bitbucket_destroy(void *userdata)
{
	(void)userdata;

#if 0
	bitbucket_user_data_t *BBud = (bitbucket_user_data_t *)userdata;
	unsigned index = 0;

	// Let's undo the work that we did in init.
	// Note: this is going to crash if the volume isn't cleanly torn down, so that's probabl
	// not viable long term.  Good for testing, though.

	for(index = sizeof(BitbucketMagicNames)/ sizeof(const char *); index > 0; ) {
		index--;

		if (0 == strncmp("Unused", BitbucketMagicNames[index], 6)) {
			continue; // skip these for now.
		}

		BitbucketDeleteDirectory(BBud->BitbucketMagicDirectories[index].Inode);
		BitbucketDereferenceInode(BBud->BitbucketMagicDirectories[index].Inode, INODE_LOOKUP_REFERENCE); // undo original create ref
		BBud->BitbucketMagicDirectories[index].Inode = NULL;
	}

	// This is where we have to "come clean".
	BitbucketDestroyInodeTable(BBud->InodeTable);
	BBud->InodeTable = NULL;
#endif // 0

}
