//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"

void bitbucket_lseek(fuse_req_t req, fuse_ino_t ino, off_t off, int whence, struct fuse_file_info *fi)
{

	(void) ino;
	(void) whence;
	(void) fi;

	// This is a no-op for us
	fuse_reply_lseek(req, off);

}
