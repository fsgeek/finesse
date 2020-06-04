//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"

void bitbucket_getlk(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, struct flock *lock)
{
	(void) req;
	(void) ino;
	(void) fi;
	(void) lock;
	
	assert(0); // Not implemented
}
