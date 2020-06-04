//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"

void bitbucket_getxattr(fuse_req_t req, fuse_ino_t ino, const char *name, size_t size)
{
	(void) req;
	(void) ino;
	(void) name;
	(void) size;

	assert(0); // Not implemented
}
