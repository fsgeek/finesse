//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <uuid/uuid.h>

void bitbucket_symlink(fuse_req_t req, const char *link, fuse_ino_t parent, const char *name)
{
	(void) req;
	(void) link;
	(void) parent;
	(void) name;

	assert(0); // Not implemented
}

static void SymlinkInitialize(void *Inode, size_t Length)
{
    bitbucket_inode_t *SymlinkInode = (bitbucket_inode_t *)Inode;

	(void) Length;

    assert(NULL != Inode);
    CHECK_BITBUCKET_INODE_MAGIC(SymlinkInode);
    assert(BITBUCKET_INODE_MAGIC == SymlinkInode->Magic);
    assert(BITBUCKET_UNKNOWN_TYPE == SymlinkInode->InodeType);

    SymlinkInode->InodeType = BITBUCKET_SYMLINK_TYPE; // Mark this as being a directory
    SymlinkInode->Instance.SymbolicLink.Magic = BITBUCKET_FILE_MAGIC;
    SymlinkInode->Attributes.st_mode |= S_IFLNK; // mark as a regular file
    SymlinkInode->Attributes.st_nlink = 1; // wonder what happens if you try to hard link a symlink.
    SymlinkInode->Attributes.st_size = 0; // what does this mean for a symlink
    SymlinkInode->Attributes.st_blocks = 0; // ?
}

static void SymlinkDeallocate(void *Inode, size_t Length)
{
    bitbucket_inode_t *bbi;
    
    assert(NULL != Inode);
    bbi = (bitbucket_inode_t *)Inode;
    assert(NULL != bbi);
    assert(bbi->InodeLength == Length);
    CHECK_BITBUCKET_INODE_MAGIC(bbi);
    CHECK_BITBUCKET_SYMLINK_MAGIC(&bbi->Instance.SymbolicLink); // layers of sanity checking
    bbi->Instance.SymbolicLink.Magic = ~BITBUCKET_SYMLINK_MAGIC; // make it easy to recognize use after free

    // Our state is torn down at this point. 

}

static bitbucket_object_attributes_t SymbolicLinkObjectAttributes = {
    .Magic = BITBUCKET_OBJECT_ATTRIBUTES_MAGIC,
    .Initialize = SymlinkInitialize,
    .Deallocate = SymlinkDeallocate,
    .Lock = NULL,
    .Unlock = NULL,
};


//
// Create a new symlink with the given name, insert it into the parent directory
// return a pointer to the new inode.
//
// Upon return, this inode will have a lookup reference (for the caller's )
// copy, plus a reference from the directory entry of the parent.
//
bitbucket_inode_t *BitbucketCreateSymlink(bitbucket_inode_t *Parent, const char *FileName, const char *Link)
{
    bitbucket_inode_t *newlink = NULL;
    int status = 0;
	size_t length = 0;
	size_t existing_space = sizeof(bitbucket_inode_t) - offsetof(bitbucket_inode_t, Instance.SymbolicLink.LinkContents);

    CHECK_BITBUCKET_INODE_MAGIC(Parent);
    assert(BITBUCKET_DIR_TYPE == Parent->InodeType); // don't support anything else with "contents" (for now)
    CHECK_BITBUCKET_DIR_MAGIC(&Parent->Instance.Directory);
    assert(NULL != Parent->Table); // Parent must be in a table
	assert(NULL != Link);

	// This is slightly complicated by the fact we have some space in the structure we can already use
	//
	length = strlen(Link) + 1; 
	if (length > existing_space) {
		length -= existing_space; // we need a bit extra
	}
	else {
		length = 0; // we don't need any more space
	}

	newlink = BitbucketCreateInode(Parent->Table, &SymbolicLinkObjectAttributes, length);
    assert(NULL != newlink);
    CHECK_BITBUCKET_INODE_MAGIC(newlink);
    assert(BITBUCKET_SYMLINK_TYPE == newlink->InodeType);
    CHECK_BITBUCKET_SYMLINK_MAGIC(&newlink->Instance.SymbolicLink);
    assert(S_IFLNK == (newlink->Attributes.st_mode & S_IFMT));

    // Parent points to the child (if there's a name)
    assert(NULL != FileName);

    newlink->Attributes.st_nlink = 1;
    status = BitbucketInsertDirectoryEntry(Parent, newlink, FileName);
    assert(0 == status);

    return newlink;

}

int BitbucketRemoveSymlinkFromDirectory(bitbucket_inode_t *Parent, const char *SymlinkName)
{
	int status = ENOENT;
	bitbucket_inode_t *symlink = NULL;

	assert(NULL != Parent);
	CHECK_BITBUCKET_DIR_MAGIC(Parent);

	BitbucketLookupObjectInDirectory(Parent, SymlinkName, &symlink);

	while (NULL != symlink) {

		if (S_IFDIR == (symlink->Attributes.st_mode & S_IFDIR)) {
			status = EISDIR;
			break;
		}

		assert(S_IFLNK == (symlink->Attributes.st_mode & S_IFLNK));

		status = BitbucketDeleteDirectoryEntry(Parent, SymlinkName);
		break;

	}

	if (NULL != symlink) {
		BitbucketDereferenceInode(symlink, INODE_LOOKUP_REFERENCE);
	}


	return status;
}