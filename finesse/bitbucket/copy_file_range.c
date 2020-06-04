//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"

void bitbucket_copy_file_range(fuse_req_t req, fuse_ino_t ino_in, off_t off_in, struct fuse_file_info *fi_in, fuse_ino_t ino_out, off_t off_out, struct fuse_file_info *fi_out, size_t len, int flags)
{
	(void) req;
	(void) ino_in;
	(void) off_in;
	(void) fi_in;
	(void) ino_out;
	(void) off_out;
	(void) fi_out;
	(void) len;
	(void) flags;

	assert(0); // Not implemented
}
