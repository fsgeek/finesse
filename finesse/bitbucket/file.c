//
// (C) Copyright 2020 Tony Mason (fsgeek@cs.ubc.ca)
// All Rights Reserved
//

#define _GNU_SOURCE

#include "bitbucket.h"
#include "bitbucketdata.h"
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <uuid/uuid.h>
#include <sys/mman.h>
#include <fcntl.h>

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
    FileInode->Instance.File.Map = NULL;
    FileInode->Attributes.st_mode |= S_IFREG; // mark as a regular file
    FileInode->Attributes.st_nlink = 0;
    FileInode->Attributes.st_size = 0;
    FileInode->Attributes.st_blocks = 0;
}

static void FileDeallocate(void *Inode, size_t Length)
{
    bitbucket_inode_t *bbi;
    int status = 0;

    assert(NULL != Inode);
    bbi = (bitbucket_inode_t *)Inode;
    assert(NULL != bbi);
    assert(bbi->InodeLength == Length);
    CHECK_BITBUCKET_INODE_MAGIC(bbi);
    CHECK_BITBUCKET_FILE_MAGIC(&bbi->Instance.File); // layers of sanity checking

    if (NULL != bbi->Instance.File.Map)
    {
        // Can't reference the inode at this point, so we have to manually dismember
        // the map here (versus call adjust length)
        if (NULL == bbi->Instance.File.MapName)
        {
            free(bbi->Instance.File.Map);
            bbi->Instance.File.Map = NULL;
        }
        else
        {
            status = munmap(bbi->Instance.File.Map, bbi->Attributes.st_size);
            assert(0 == status); // must be a programming bug...
        }
    }

    if (NULL != bbi->Instance.File.MapName)
    {
        status = unlink(bbi->Instance.File.MapName);
        assert(0 == status); // if not, probably a program bug.
        free(bbi->Instance.File.MapName);
        bbi->Instance.File.MapName = NULL;
    }

    bbi->Instance.File.Magic = ~BITBUCKET_FILE_MAGIC; // make it easy to recognize use after free

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
bitbucket_inode_t *BitbucketCreateFile(bitbucket_inode_t *Parent, const char *FileName, bitbucket_userdata_t *BBud)
{
    bitbucket_inode_t *newfile = NULL;
    int status = 0;
    const char *storage_dir = NULL;

    CHECK_BITBUCKET_INODE_MAGIC(Parent);
    assert(BITBUCKET_DIR_TYPE == Parent->InodeType); // don't support anything else with "contents" (for now)
    CHECK_BITBUCKET_DIR_MAGIC(&Parent->Instance.Directory);
    assert(NULL != Parent->Table); // Parent must be in a table
    if (NULL != BBud)
    {
        CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);
        storage_dir = BBud->StorageDir;
    }

    newfile = BitbucketCreateInode(Parent->Table, &FileObjectAttributes, 0);
    assert(NULL != newfile);
    CHECK_BITBUCKET_INODE_MAGIC(newfile);
    assert(BITBUCKET_FILE_TYPE == newfile->InodeType);
    CHECK_BITBUCKET_FILE_MAGIC(&newfile->Instance.File);
    assert(S_IFREG == (newfile->Attributes.st_mode & S_IFMT));

    if (NULL != storage_dir)
    {
        // Create a location for storage
        size_t size = strlen(storage_dir) + strlen(newfile->UuidString) + 3;
        size_t used = size + 1;
        char *storage_name = malloc(size);
        int fd = -1;

        used = snprintf(storage_name, size, "%s/%s", storage_dir, newfile->UuidString);
        assert(used < size);                                      // if not, the logic here was wrong
        fd = open(storage_name, O_RDWR | O_CREAT | O_EXCL, 0600); // shouldn't already exist!
        assert(fd >= 0);
        close(fd);
        fd = -1;
        newfile->Instance.File.MapName = storage_name;
        storage_name = NULL;
    }

    // Parent points to the child (if there's a name)
    assert(NULL != FileName);

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
    status = BitbucketInsertDirectoryEntry(Parent, File, FileName);
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

    while (NULL != file)
    {
        if (S_IFDIR == (file->Attributes.st_mode & S_IFDIR))
        {
            status = EISDIR;
            break;
        }

        // definitely shouldn't be called here.
        assert(S_IFREG == (file->Attributes.st_mode & S_IFREG));

        assert(file->Attributes.st_nlink > 0); // shouldn't go negative
        status = BitbucketDeleteDirectoryEntry(Parent, FileName);
        if (0 != status)
        {
            break;
        }

        break;
    }

    if (NULL != file)
    {
        BitbucketDereferenceInode(file, INODE_LOOKUP_REFERENCE, 1);
        file = NULL;
    }

    return status;
}

//
// Given a (locked) Inode, this
// function will adjust it's length appropriately
//
int BitbucketAdjustFileStorage(bitbucket_inode_t *Inode, size_t NewLength)
{
    int status = ENOSPC;
    int fd = -1;
    void *newbuf = NULL;

    assert(NULL != Inode);
    assert(BITBUCKET_FILE_TYPE == Inode->InodeType);
    CHECK_BITBUCKET_FILE_MAGIC(&Inode->Instance.File);

    EnsureInodeLockedAgainstChanges(Inode);
    while (NULL != Inode)
    {
        if (NewLength == Inode->Attributes.st_size)
        {
            // No work to do in this case
            status = 0;
            break;
        }

        if (NULL == Inode->Instance.File.Map)
        {
            // Create a new mapping
            if (NULL == Inode->Instance.File.MapName)
            {
                Inode->Instance.File.Map = malloc(NewLength);

                if (NULL == Inode->Instance.File.Map)
                {
                    status = ENOSPC;
                    break;
                }

                memset(Inode->Instance.File.Map, 0, NewLength);
                status = 0;
                break;
            }

            fd = open(Inode->Instance.File.MapName, O_RDWR | O_CREAT, 0600);

            if (fd < 0)
            {
                status = errno;
                break;
            }

            // Make sure the file is big enough.
            status = posix_fallocate(fd, 0, NewLength);
            if (0 != status)
            {
                break;
            }

            Inode->Instance.File.Map = mmap(NULL, NewLength, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (MAP_FAILED == Inode->Instance.File.Map)
            {
                status = ENOSPC;
                break;
            }

            Inode->Attributes.st_size = NewLength;
            // sloppy block length
            Inode->Attributes.st_blocks = 1 + (Inode->Attributes.st_size / Inode->Attributes.st_blksize);
            status = 0;
            break;
        }

        if (0 == NewLength)
        {

            if (NULL == Inode->Instance.File.MapName)
            {
                // memory was allocated
                free(Inode->Instance.File.Map);
                status = 0;
            }
            else
            {
                status = munmap(Inode->Instance.File.Map, NewLength);
            }

            if (0 != status)
            {
                // error path, no changes
                break;
            }

            Inode->Instance.File.Map = NULL;
            Inode->Attributes.st_size = 0;
            Inode->Attributes.st_blocks = 0;
            status = 0;
            break;
        }

        // Otherwise we already have a mapping, we're just adjusting its size
        if (NULL == Inode->Instance.File.MapName)
        {
            // this was dynamically allocated.
            newbuf = realloc(Inode->Instance.File.Map, NewLength);
            if (NULL == newbuf)
            {
                status = ENOSPC;
            }
            else
            {
                status = 0;
            }
            break;
        }

        // At this point it was created using mmap
        fd = open(Inode->Instance.File.MapName, O_RDWR); // it must exit
        assert(fd >= 0);                                 // it should exist
        status = posix_fallocate(fd, 0, NewLength);
        if (0 != status)
        {
            break;
        }
        newbuf = mmap(NULL, NewLength, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (MAP_FAILED == newbuf)
        {
            assert(0); // figure out if this is OK or not
            status = ENOSPC;
            break;
        }
        status = munmap(Inode->Instance.File.Map, Inode->Attributes.st_size);
        assert(0 == status); // if not, write more plumbing
        Inode->Instance.File.Map = newbuf;
        newbuf = NULL;
        Inode->Attributes.st_size = NewLength;
        // sloppy block length
        Inode->Attributes.st_blocks = 1 + (Inode->Attributes.st_size / Inode->Attributes.st_blksize);
        status = 0;
        break;
    }

    if (fd >= 0)
    {
        close(fd);
        fd = -1;
    }

    if (NULL != newbuf)
    {
        free(newbuf);
        newbuf = NULL;
    }

    return status;
}
