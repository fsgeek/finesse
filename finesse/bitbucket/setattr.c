//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"

void bitbucket_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr, int to_set, struct fuse_file_info *fi)
{
	(void) req;
	(void) ino;
	(void) attr;
	(void) to_set;
	(void) fi;

	assert(0); // Not implemented
}
