//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include <errno.h>
#include <sys/statfs.h>
#include <string.h>

void bitbucket_statfs(fuse_req_t req, fuse_ino_t ino)
{

	bitbucket_inode_t *inode = NULL;
	bitbucket_userdata_t *bbud = NULL;
	int status = EBADF;
	struct statvfs fsstat;

	assert(NULL != req);
	assert(0 != ino);

	bbud = (bitbucket_userdata_t *)fuse_req_userdata(req);
	assert(NULL != bbud);
	CHECK_BITBUCKET_USER_DATA_MAGIC(bbud);

	inode = BitbucketLookupInodeInTable(bbud->InodeTable, ino);

	while (NULL != inode) {
		memset(&fsstat, 0, sizeof(fsstat));
		fsstat.f_bsize = inode->Attributes.st_blksize;
		fsstat.f_frsize = fsstat.f_bsize;
		fsstat.f_blocks = 0; // size of fs in f_frsize units
		fsstat.f_bfree = 0;  // number of free blocks
		fsstat.f_bavail = 0; // number of free blocks for unprivileged users
		fsstat.f_files = 0;  // number of inodes
		fsstat.f_ffree = 0;  // number of free inodes
		fsstat.f_favail = 0; // number of free inodes for unprivileged users 
		fsstat.f_fsid = 42; // no idea where this originates

		// Options for flags are:
		//  ST_MANDLOCK - Mandatory locking is permitted on the filesystem (see fcntl(2)).
		//  ST_NOATIME -  Do not update access times; see mount(2).
		//  ST_NODEV - Disallow access to device special files on this filesystem.
		//  ST_NODIRATIME - Do not update directory access times; see mount(2).
		//  ST_NOEXEC - Execution of programs is disallowed on this filesystem.
		//  ST_NOSUID - The set-user-ID and set-group-ID bits are ignored by exec(3) for executable files on this filesystem
		//  ST_RDONLY - This filesystem is mounted read-only.
		//  ST_RELATIME - Update atime relative to mtime/ctime; see mount(2).
		//  ST_SYNCHRONOUS - Writes are synched to the filesystem immediately (see the description of O_SYNC in open(2)).
		fsstat.f_flag = 0;

		fsstat.f_namemax = MAX_FILE_NAME_SIZE;

	}

	if (NULL != inode) {
		BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE);
		inode = NULL;
	}

	if (0 != status) {
		fuse_reply_err(req, status);
		return;
	}

}
