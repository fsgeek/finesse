//
// (C) Copyright 2020 Tony Mason (fsgeek@cs.ubc.ca)
// All Rights Reserved
//

#include "bitbucket.h"
#include "bitbucketdata.h"
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <uuid/uuid.h>

static void FileInitialize(void *Inode, size_t Length)
{
    bitbucket_inode_t *FileInode = (bitbucket_inode_t *)Inode;

    assert(NULL != Inode);
    CHECK_BITBUCKET_INODE_MAGIC(FileInode);
    assert(BITBUCKET_INODE_MAGIC == FileInode->Magic);
    assert(BITBUCKET_UNKNOWN_TYPE == FileInode->InodeType);
    assert(Length == FileInode->InodeLength);

    FileInode->InodeType = BITBUCKET_FILE_TYPE; // Mark this as being a directory
    FileInode->Instance.File.Magic = BITBUCKET_FILE_MAGIC;
    FileInode->Attributes.st_mode |= S_IFREG; // mark as a regular file
    FileInode->Attributes.st_nlink = 0;
    FileInode->Attributes.st_size = 0;
    FileInode->Attributes.st_blocks = 0;
}

static void FileDeallocate(void *Inode, size_t Length)
{
    bitbucket_inode_t *bbi;
    
    assert(NULL != Inode);
    bbi = (bitbucket_inode_t *)Inode;
    assert(NULL != bbi);
    assert(bbi->InodeLength == Length);
    CHECK_BITBUCKET_INODE_MAGIC(bbi);
    CHECK_BITBUCKET_FILE_MAGIC(&bbi->Instance.File); // layers of sanity checking
    bbi->Instance.File.Magic = ~BITBUCKET_DIR_MAGIC; // make it easy to recognize use after free

    // Our state is torn down at this point. 

}

#if 0
static void FileLock(void *Inode, int Exclusive)
{
    bitbucket_inode_t *bbi = (bitbucket_inode_t *)Inode;
    int status = 0;

    assert(NULL != bbi);
    CHECK_BITBUCKET_INODE_MAGIC(bbi);
    CHECK_BITBUCKET_FILE_MAGIC(&bbi->Instance.File);

    if (Exclusive) {
        status = pthread_rwlock_wrlock(&bbi->InodeLock);
        assert(0 == status);
    }
    else {
        pthread_rwlock_rdlock(&bbi->InodeLock);
        assert(0 == status);
    }

}

static void FileUnlock(void *Inode)
{
    int status;
    bitbucket_inode_t *bbi = (bitbucket_inode_t *)Inode;

    assert(NULL != bbi);
    CHECK_BITBUCKET_INODE_MAGIC(bbi);
    CHECK_BITBUCKET_FILE_MAGIC(&bbi->Instance.File);

    status = pthread_rwlock_unlock(&bbi->InodeLock);
    assert(0 == status);
}
#endif // 0

static bitbucket_object_attributes_t FileObjectAttributes = {
    .Magic = BITBUCKET_OBJECT_ATTRIBUTES_MAGIC,
    .Initialize = FileInitialize,
    .Deallocate = FileDeallocate,
    .Lock = NULL,
    .Unlock = NULL,
};

//
// Create a new file with the given name, insert it into the parent directory
// return a pointer to the new file inode.
//
// Upon return, this inode will have a lookup reference (for the caller's )
// copy, plus a reference from the the directory entry of the parent.
//
bitbucket_inode_t *BitbucketCreateFile(bitbucket_inode_t *Parent, const char *FileName)
{
    bitbucket_inode_t *newfile = NULL;
    int status = 0;

    CHECK_BITBUCKET_INODE_MAGIC(Parent);
    assert(BITBUCKET_DIR_TYPE == Parent->InodeType); // don't support anything else with "contents" (for now)
    CHECK_BITBUCKET_DIR_MAGIC(&Parent->Instance.Directory);
    assert(NULL != Parent->Table); // Parent must be in a table

    newfile = BitbucketCreateInode(Parent->Table, &FileObjectAttributes, 0);
    assert(NULL != newfile);
    CHECK_BITBUCKET_INODE_MAGIC(newfile);
    assert(BITBUCKET_FILE_TYPE == newfile->InodeType);
    CHECK_BITBUCKET_FILE_MAGIC(&newfile->Instance.File);
    assert(S_IFREG == (newfile->Attributes.st_mode & S_IFMT));

    // Parent points to the child (if there's a name)
    assert(NULL != FileName);

    newfile->Attributes.st_nlink = 1;
    status = BitbucketInsertDirectoryEntry(Parent, newfile, FileName);
    assert(0 == status);

    return newfile;

}

//
// This is used to create a new link for an existing file.
//
int BitbucketAddFileToDirectory(bitbucket_inode_t *Parent, bitbucket_inode_t *File, const char *FileName)
{
    int status = 0;

    assert(NULL != Parent);
    assert(NULL != File);
    CHECK_BITBUCKET_FILE_MAGIC(&File->Instance.File);
    CHECK_BITBUCKET_DIR_MAGIC(&Parent->Instance.Directory);

    BitbucketLockInode(Parent, 1);
    BitbucketLockInode(File, 1);
    File->Attributes.st_nlink = 1;
    status = BitbucketInsertDirectoryEntry(Parent, File, FileName);
    BitbucketUnlockInode(File);
    BitbucketUnlockInode(Parent);

    return status;
}

//
// Removes a file from the specified directory, using the specified name
//
// Note that if the link count of the file goes to zero, it will be removed from
// the inode table at this point.
//
// Thus, there is no longer a table reference to this file (assuming it exists)
//
int BitbucketRemoveFileFromDirectory(bitbucket_inode_t *Parent, const char *FileName)
{
    int status = ENOENT;
    bitbucket_inode_t *file = NULL;

    assert(NULL != Parent);
    CHECK_BITBUCKET_DIR_MAGIC(&Parent->Instance.Directory);

    BitbucketLookupObjectInDirectory(Parent, FileName, &file);

    while (NULL != file) {
        if (S_IFDIR == (file->Attributes.st_mode & S_IFDIR)) {
            status = EISDIR;
            break;
        }

        status = BitbucketDeleteDirectoryEntry(Parent, FileName);
        if (0 != status) {
            break;
        }
        assert(file->Attributes.st_nlink > 0); // shouldn't go negative
        BitbucketLockInode(file, 1);
        file->Attributes.st_nlink--;
        BitbucketUnlockInode(file);

        break;
    }

    if (NULL != file) {       
        BitbucketDereferenceInode(file, INODE_LOOKUP_REFERENCE);
        file = NULL;
    }

    return status;    


}

#if 0
//
// Invoke this when you want to initiate teardown of a file.
// The link count must be zero (Attributes.st_nlink)
// 
// Note: the caller should own a reference when calling this function.  The caller should
// release that reference after this call returns (which could trigger deletion).  It
// should work without that reference, but the Inode pointer may become invalid.
//
int BitbucketDeleteFile(bitbucket_inode_t *Inode)
{
    int status = 0;
    assert(NULL != Inode);
    CHECK_BITBUCKET_INODE_MAGIC(Inode);
    assert(BITBUCKET_FILE_TYPE == Inode->InodeType);
    CHECK_BITBUCKET_FILE_MAGIC(&Inode->Instance.File);

    while (NULL != Inode) {

        // Must be zero.  Otherwise there's still a dir reference
        assert(0 == Inode->Attributes.st_nlink); 
        BitbucketRemoveInodeFromTable(Inode);

        break;
    }
    
    return status;
}
#endif // 0

