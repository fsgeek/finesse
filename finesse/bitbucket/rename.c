//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"

void bitbucket_rename(fuse_req_t req, fuse_ino_t parent, const char *name, fuse_ino_t newparent, const char *newname, unsigned int flags)
{
	(void) req;
	(void) parent;
	(void) name;
	(void) newparent;
	(void) newname;
	(void) flags;

	assert(0); // Not implemented
}
