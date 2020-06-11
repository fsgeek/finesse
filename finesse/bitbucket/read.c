//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include <errno.h>
#include <malloc.h>

#define PAGE_SIZE (4096)

static const char zeropagebuf[PAGE_SIZE];
static const void *zeropage = (void *)zeropagebuf;

void bitbucket_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi)
{
	void *userdata = fuse_req_userdata(req);
	bitbucket_user_data_t *BBud = (bitbucket_user_data_t *)userdata;
	bitbucket_inode_t *inode = NULL;
	int status = 0;
	struct fuse_bufvec *bufv = NULL;
	unsigned int flags = FUSE_BUF_NO_SPLICE;
	size_t page_size = PAGE_SIZE; // TODO: find a better way to do this for "real world"
	unsigned page_count = 0;
	size_t bufv_size = 0;

	(void) fi;

	if (0 == size) {
		// zero byte reads always succeed
		fuse_reply_buf(req, NULL, 0);
		return;
	}

	// TODO: should we be doing anything with the flags in fi->flags?

	CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);

	if (FUSE_ROOT_ID == ino) {
		inode = BBud->RootDirectory;
		BitbucketReferenceInode(inode, INODE_LOOKUP_REFERENCE); // must have a lookup ref.
	}
	else {
		inode = BitbucketLookupInodeInTable(BBud->InodeTable, ino); // returns a lookup ref.	
	}

	//
	// This is what makes the file system a bit bucket: writes are discarded (with success, of course)
	//
	status = EBADF;
	
	while (NULL != inode) {

		if (BITBUCKET_FILE_TYPE != inode->InodeType) {
			status = EINVAL;
			break;			
		}

		if (size + off > inode->Attributes.st_size) {
			if (off > inode->Attributes.st_size) {
				size = 0;
				break;
			}
			else {
				size = inode->Attributes.st_size - off;
			}
		}

		page_count = (size + (page_size - 1)) & ~(page_size -1); // round up
		bufv_size = offsetof(struct fuse_bufvec, buf) + page_count * sizeof(struct fuse_buf);
		bufv = (struct fuse_bufvec *)malloc(bufv_size);
		assert(NULL != bufv);
		bufv->count = page_count;
		bufv->idx = 0; // identity of current page
		bufv->off = 0; // offset within the buffer
		for (unsigned index = 0; index < page_count; index++) {
			bufv->buf[index].mem = (void *)(uintptr_t)zeropage;
			bufv->buf[index].fd = -1;
			bufv->buf[index].flags = 0;
			bufv->buf[index].pos = 0; // unused
			bufv->buf[index].size = page_size;
		}
		// adjust the size of the last page
		bufv->buf[page_count-1].size = size & (page_size - 1);
		status = 0; // success
		break;
	}

	if (0 == status) {

		if (0 == size) {
			fuse_reply_buf(req, NULL, 0);
		}
		else {
			assert(NULL != bufv);
			fuse_reply_data(req, bufv, flags);
		}
	}
	else {
		fuse_reply_err(req, status);
	}

	if (NULL != bufv) {
		free(bufv);
	}

	if (NULL != inode) {
		BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE);
		inode = NULL;

	}

}
