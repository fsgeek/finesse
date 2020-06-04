//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"

void bitbucket_poll(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, struct fuse_pollhandle *ph)
{
	(void) req;
	(void) ino;
	(void) fi;
	(void) ph;
	
	assert(0); // Not implemented
}
