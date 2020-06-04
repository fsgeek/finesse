//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"

void bitbucket_flock(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, int op)
{
	(void) req;
	(void) ino;
	(void) fi;
	(void) op;

	assert(0); // Not implemented
}
