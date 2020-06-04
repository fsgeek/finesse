//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"

void bitbucket_symlink(fuse_req_t req, const char *link, fuse_ino_t parent, const char *name)
{
	(void) req;
	(void) link;
	(void) parent;
	(void) name;

	assert(0); // Not implemented
}
