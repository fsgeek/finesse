//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"

void bitbucket_ioctl(fuse_req_t req, fuse_ino_t ino, unsigned int cmd, void *arg, struct fuse_file_info *fi, unsigned flags, const void *in_buf, size_t in_bufsz, size_t out_bufsz)
{
	(void) req;
	(void) ino;
	(void) cmd;
	(void) flags;
	(void) in_buf;
	(void) in_bufsz;
	(void) out_bufsz;
	(void) arg;
	(void) fi;

	assert(0); // Not implemented
}
