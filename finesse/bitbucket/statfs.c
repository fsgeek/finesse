//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include "bitbucketcalls.h"
#include <errno.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>

static int bitbucket_internal_statfs(fuse_req_t req, fuse_ino_t ino);


void bitbucket_statfs(fuse_req_t req, fuse_ino_t ino)
{
	struct timespec start, stop, elapsed;
	int status, tstatus;

	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	assert(0 == tstatus);
	status = bitbucket_internal_statfs(req, ino);
	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
	assert(0 == tstatus);
	timespec_diff(&start, &stop, &elapsed);
	BitbucketCountCall(BITBUCKET_CALL_STATFS, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_statfs(fuse_req_t req, fuse_ino_t ino)
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

	if (FUSE_ROOT_ID == ino) {
		inode = bbud->RootDirectory;
		BitbucketReferenceInode(inode, INODE_LOOKUP_REFERENCE);
	}
	else {
		inode = BitbucketLookupInodeInTable(bbud->InodeTable, ino);
	}

	while (NULL != inode) {

		memset(&fsstat, 0, sizeof(fsstat));

		// common values first
		fsstat.f_bsize = inode->Attributes.st_blksize;
		fsstat.f_frsize = fsstat.f_bsize;
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

		// now specific values
		if (bbud->StorageDir) {
			struct statvfs sdstat;

			status = statvfs(bbud->StorageDir, &sdstat);
			assert(0 == status); // if not, logic issue
			fsstat.f_blocks = sdstat.f_blocks; // size of fs in f_frsize units
			fsstat.f_bfree = sdstat.f_bfree; // number of free blocks
			fsstat.f_bavail = sdstat.f_bavail; // available free blocks for unprivileged users
			fsstat.f_files = sdstat.f_files; // number of inodes
			fsstat.f_ffree = sdstat.f_ffree; // number of free inodes
		}
		else {
			struct rlimit rlim;
			struct rusage usage;
			uint64_t count = BitbucketGetInodeTableCount(inode->Table);

			status = getrusage(RUSAGE_SELF, &usage);
			assert(0 == status);
			status = getrlimit(RLIMIT_DATA, &rlim);
			assert(0 == status);

			fsstat.f_blocks = rlim.rlim_cur / fsstat.f_bsize;
			fsstat.f_bfree = (rlim.rlim_cur - (usage.ru_ixrss + usage.ru_idrss + usage.ru_isrss)) / fsstat.f_bsize;
			fsstat.f_bavail = (90 * fsstat.f_bfree) / 100; // 90%
			fsstat.f_files = (10 * fsstat.f_bfree) / 100; // 10%
			if (count > fsstat.f_files) {
				fsstat.f_ffree = count - fsstat.f_files;
			}
			else {
				fsstat.f_files = count + 1;
				fsstat.f_ffree = 1;
			}
		}
		fsstat.f_bavail = fsstat.f_bavail; // number of free blocks for unprivileged users
		fsstat.f_favail = fsstat.f_favail; // number of free inodes for unprivileged users
		status = 0;
		break;
	}

	if (NULL != inode) {
		BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE, 1);
		inode = NULL;
	}

	if (0 != status) {
		fuse_reply_err(req, status);
		return status;
	}
	else {
		fuse_reply_statfs(req, &fsstat);
	}

	return status;

}
