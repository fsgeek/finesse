//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"

void bitbucket_mknod(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, dev_t rdev)
{
	(void) req;
	(void) parent;
	(void) mode;
	(void) name;
	(void) rdev;

	assert(0); // Not implemented
}
