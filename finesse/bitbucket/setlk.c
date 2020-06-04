//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"

void bitbucket_setlk(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, struct flock *lock, int sleep)
{
	(void) req;
	(void) ino;
	(void) lock;
	(void) sleep;
	(void) fi;

	assert(0); // Not implemented
}
