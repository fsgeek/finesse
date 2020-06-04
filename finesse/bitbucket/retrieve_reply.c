//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"

void bitbucket_retrieve_reply(fuse_req_t req, void *cookie, fuse_ino_t ino, off_t offset, struct fuse_bufvec *bufv)
{
	(void) req;
	(void) ino;
	(void) cookie;
	(void) offset;
	(void) bufv;

	assert(0); // Not implemented
}
