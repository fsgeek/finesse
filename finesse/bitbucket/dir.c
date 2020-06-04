//
// (C) Copyright 2020 Tony Mason (fsgeek@cs.ubc.ca)
// All Rights Reserved
//

#include "bitbucket.h"
#include "trie.h"
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

static ino_t nextInode = 1024; // arbitrary, really

bitbucket_dir_t *BitbucketCreateDirectory(bitbucket_dir_t *Parent, const char *DirName)
{
    bitbucket_dir_t *newdir = (bitbucket_dir_t *)malloc(sizeof(bitbucket_dir_t));
    int status;
    
    assert(NULL != newdir);

    uuid_generate(newdir->DirId);
    uuid_unparse(newdir->DirId, newdir->DirIdName);

    newdir->Entries = TrieCreateNode();
    assert(NULL != newdir->Entries);

    // Get our timestamps
    status = gettimeofday(&newdir->CreationTime, NULL);
    assert(0 == status);
    newdir->AccessTime = newdir->CreationTime;
    newdir->ModifiedTime = newdir->CreationTime;
    newdir->ChangeTime = newdir->CreationTime;

    // make sure we zero everything out.  We will set some fields below
    memset(&newdir->Attributes, 0, sizeof(struct stat));

    newdir->Attributes.st_mode = S_IRWXU | S_IRWXG | S_IRWXO | S_IFDIR; // dir with all access bit - TODO - parameterize? 
    newdir->Attributes.st_gid = getgid();
    newdir->Attributes.st_uid = getuid();
    newdir->Attributes.st_size = 4096; // TODO: maybe do some sort of funky calculation here?
    newdir->Attributes.st_blksize = 4096; // TODO: again, does this matter?
    newdir->Attributes.st_blocks = newdir->Attributes.st_size / 512;
    newdir->Attributes.st_atime = newdir->AccessTime.tv_sec;
    newdir->Attributes.st_mtime = newdir->ModifiedTime.tv_sec;
    newdir->Attributes.st_ctime = newdir->ChangeTime.tv_sec;

    if (NULL == Parent) {
        newdir->Parent = newdir;
        newdir->Attributes.st_ino = FUSE_ROOT_ID; // root directory
        newdir->Attributes.st_nlink = 2; // . and ..
    }
    else {
        newdir->Parent = Parent;
        newdir->Attributes.st_ino =  __atomic_fetch_add(&nextInode, 11, __ATOMIC_RELAXED); // again, arbitrary bias
        newdir->Attributes.st_nlink = 1; // .
        TrieInsert(Parent->Entries, DirName, newdir);
    }


    return newdir;
}