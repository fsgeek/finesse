//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"

void bitbucket_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi)
{
	(void)req;
	(void)parent;
	(void)name;
	(void)mode;
	(void)fi;
	
	assert(0); // Not implemented
}
