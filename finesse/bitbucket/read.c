//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"

void bitbucket_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi)
{
	(void) req;
	(void) ino;
	(void) size;
	(void) off;
	(void) fi;

	assert(0); // Not implemented
}
