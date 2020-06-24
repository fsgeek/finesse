//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include "bitbucketcalls.h"
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <uuid/uuid.h>

static int bitbucket_internal_symlink(fuse_req_t req, const char *link, fuse_ino_t parent, const char *name);


void bitbucket_symlink(fuse_req_t req, const char *link, fuse_ino_t parent, const char *name)
{
	struct timespec start, stop, elapsed;
	int status, tstatus;

	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	assert(0 == tstatus);
	status = bitbucket_internal_symlink(req, link, parent, name);
	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
	assert(0 == tstatus);
	timespec_diff(&start, &stop, &elapsed);
	BitbucketCountCall(BITBUCKET_CALL_SYMLINK, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_symlink(fuse_req_t req, const char *link, fuse_ino_t parent, const char *name)
{
    int status = EACCES;
    bitbucket_inode_t *inode = NULL;
    bitbucket_inode_t *symlinkInode = NULL;
   	void *userdata = NULL;
	bitbucket_userdata_t *BBud = NULL;
	struct fuse_entry_param fep;

    assert(NULL != req);
    assert(NULL != link);
    assert(0 != strlen(link));
    assert(0 != parent);
    assert(NULL != name);
    assert(0 != strlen(name));

    userdata = fuse_req_userdata(req);
    assert(NULL != userdata);
    BBud = (bitbucket_userdata_t *)userdata;

    while (0 != status) {
        inode = BitbucketLookupInodeInTable(BBud->InodeTable, parent);
        if (NULL == inode) {
            status = EBADF;
            break; // invalid inode number
        }

        symlinkInode = BitbucketCreateSymlink(inode, name, link);
        if (NULL == symlinkInode) {
            status = EACCES;
            break; // not sure why - so we say "access denied"
        }

        status = 0;
        break;
    }

    if (NULL != inode) {
        BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE);
        inode = NULL;
    }

    // TODO: build the entry and return it.
    if (0 != status) {
        fuse_reply_err(req, status);
        return status;
    }

    assert(0 == status);
    assert(NULL != symlinkInode);
    memset(&fep, 0, sizeof(fep));
    fep.ino = symlinkInode->Attributes.st_ino;
    fep.generation = symlinkInode->Epoch;
    fep.attr = symlinkInode->Attributes;
    fep.attr_timeout = 30;
    fep.entry_timeout = 30;
    fuse_reply_entry(req, &fep);

    BitbucketReferenceInode(symlinkInode, INODE_FUSE_LOOKUP_REFERENCE); // this matches the fuse_reply_entry
    BitbucketDereferenceInode(symlinkInode, INODE_LOOKUP_REFERENCE);

    return status;

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
    SymlinkInode->Instance.SymbolicLink.Magic = BITBUCKET_SYMLINK_MAGIC;
    SymlinkInode->Attributes.st_mode |= S_IFLNK; // mark as a regular file
    SymlinkInode->Attributes.st_nlink = 0; // wonder what happens if you try to hard link a symlink.
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
    newlink->Attributes.st_mode |= 0777; // "On Linux the permissions of a symlink ... are always 0777"

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

int BitbucketReadSymlink(bitbucket_inode_t *Inode, const char **SymlinkContents)
{
    int status = EBADF;
    assert(NULL != Inode);
    CHECK_BITBUCKET_INODE_MAGIC(Inode);

    while (NULL != Inode) {
        if (BITBUCKET_SYMLINK_TYPE != Inode->InodeType) {
            // We can't return the contents of something that's not a symlink
            break;
        }

		assert(S_IFLNK == (Inode->Attributes.st_mode & S_IFLNK));

        *SymlinkContents = Inode->Instance.SymbolicLink.LinkContents;
        status = 0;
        break;
    }

    return status;
}