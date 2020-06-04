//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"

void bitbucket_lseek(fuse_req_t req, fuse_ino_t ino, off_t off, int whence, struct fuse_file_info *fi)
{

	(void) req;
	(void) ino;
	(void) off;
	(void) whence;
	(void) fi;

	assert(0); // Not implemented
}
