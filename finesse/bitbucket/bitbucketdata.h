//
// (C) Copyright 2020 Tony Mason (fsgeek@cs.ubc.ca)
// All Rights Reserved
//

#if !defined(_BITBUCKET_DATA_H_)
#define _BITBUCKET_DATA_H_ (1)

#if !defined(FUSE_USE_VERSION)
#define FUSE_USE_VERSION 39
#endif // FUSE_USE_VERSION

#include <fuse_lowlevel.h>
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <uuid/uuid.h>
#include "trie.h"

// Arbitrary limits of what we will support.
#define MAX_FILE_NAME_SIZE (1024)
#define MAX_PATH_NAME_SIZE (65536)

// Common data structures used within the bitbucket file system.
static inline void verify_magic(const char *StructureName, const char *File, const char *Function, unsigned Line, uint64_t Magic, uint64_t CheckMagic)
{
    if (Magic != CheckMagic) {
        fprintf(stderr, "%s (%s:%d) Magic number mismatch (%lx != %lx) for %s\n", Function, File, Line, Magic, CheckMagic, StructureName);
        assert(Magic == CheckMagic);
    }
}

typedef struct _bitbucket_file {
    uint64_t            Magic; // magic number
    pthread_rwlock_t    Lock;
    uuid_t              FileId;
    char                FileIdName[40];
    struct stat         Attributes;
} bitbucket_file_t;

#define BITBUCKET_FILE_MAGIC (0x3f917344ab19c2d8)

#define CHECK_BITBUCKET_FILE_MAGIC(bbf) verify_magic("bitbucket_file_t", __FILE__, __PRETTY_FUNCTION__, __LINE__, BITBUCKET_FILE_MAGIC, bbf->Magic)

typedef struct _bitbucket_dir {
    uint64_t              Magic; // magic number
    pthread_rwlock_t      Lock;
    uuid_t                DirId;
    char                  DirIdName[40];
    struct _bitbucket_dir *Parent;
    struct stat           Attributes;
    struct timeval        AccessTime;
    struct timeval        ModifiedTime;
    struct timeval        CreationTime;
    struct timeval        ChangeTime;
    struct Trie           *Entries;
} bitbucket_dir_t;

#define BITBUCKET_DIR_MAGIC (0x2d11f7649ac23e85)
#define CHECK_BITBUCKET_DIR_MAGIC(bbd) verify_magic("bitbucket_dir_t", __FILE__, __PRETTY_FUNCTION__, __LINE__, BITBUCKET_DIR_MAGIC, bbd->Magic)


#endif // _BITBUCKET_DATA_H_
