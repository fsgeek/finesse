/*
  Copyright (C) 2020  Tony Mason <fsgeek@cs.ubc.ca>
*/

#define _GNU_SOURCE

#include "config.h"
#include "fuse_i.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include "fuse_kernel.h"
#pragma GCC diagnostic pop
#include "fuse_misc.h"
#include "fuse_opt.h"

#include <fuse_lowlevel.h>

#include "finesse-fuse.h"

#include <finesse-server.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if !defined(VARIABLE_IS_NOT_USED)
#ifdef __GNUC__
#define VARIABLE_IS_NOT_USED __attribute__((unused))
#else
#define VARIABLE_IS_NOT_USED
#endif
#endif  // VARIABLE_IS_NOT_USED

typedef struct finesse_lookup_info {
    int status;

    /* used so the caller can wait for completion */
    pthread_mutex_t lock;
    pthread_cond_t  condition;
    int             completed;

    /* attributes */
    struct fuse_attr attr;

} finesse_lookup_info_t;

typedef struct _FinesseServerPathResolutionParameters FinesseServerPathResolutionParameters_t;

int FinesseServerInternalMapRequest(struct fuse_session *se, ino_t ParentInode, uuid_t *ParentUuid, const char *Name, int Flags,
                                    finesse_object_t **Finobj);
int FinesseServerInternalNameLookup(struct fuse_session *se, fuse_ino_t Parent, const char *Name, struct statx *attr);
int FinesseServerResolvePathName(struct fuse_session *se, FinesseServerPathResolutionParameters_t *Parameters);
FinesseServerPathResolutionParameters_t *FinesseAllocateServerPathResolutionParameters(fuse_ino_t ParentInode, const char *PathName,
                                                                                       int Flags);
void FinesseFreeServerPathResolutionParameters(FinesseServerPathResolutionParameters_t *Parameters);
int  FinesseGetResolvedStatx(FinesseServerPathResolutionParameters_t *Parameters, struct statx *StatxData);
int  FinesseGetResolvedInode(FinesseServerPathResolutionParameters_t *Parameters, ino_t *InodeNumber);

extern FinesseServerStat *FinesseServerStats;

VARIABLE_IS_NOT_USED static inline void FinesseCountNativeRequest(FINESSE_NATIVE_REQ_TYPE Type)
{
    unsigned index = (unsigned)~0;

    // Make sure it is in range
    assert(FINESSE_NATIVE_REQ_TEST <= Type);
    assert(FINESSE_NATIVE_REQ_MAX > Type);

    index = Type - FINESSE_NATIVE_REQ_TEST;
    __sync_fetch_and_add(&FinesseServerStats->NativeRequests[index], 1);
}

VARIABLE_IS_NOT_USED static inline void FinesseCountNativeResponse(FINESSE_NATIVE_RSP_TYPE Type)
{
    unsigned index = (unsigned)~0;

    // Make sure it is in range
    assert(FINESSE_NATIVE_RSP_ERR <= Type);
    assert(FINESSE_NATIVE_RSP_MAX > Type);

    index = Type - FINESSE_NATIVE_RSP_ERR;
    __sync_fetch_and_add(&FinesseServerStats->NativeResponses[index], 1);
}

VARIABLE_IS_NOT_USED static inline void FinesseCountFuseRequest(FINESSE_FUSE_REQ_TYPE Type)
{
    unsigned index = (unsigned)~0;

    // Make sure it is in range
    assert(FINESSE_FUSE_REQ_LOOKUP <= Type);
    assert(FINESSE_FUSE_REQ_MAX > Type);

    index = Type - FINESSE_FUSE_REQ_LOOKUP;
    __sync_fetch_and_add(&FinesseServerStats->FuseRequests[index], 1);
}

VARIABLE_IS_NOT_USED static void FinesseCountFuseResponse(FINESSE_FUSE_RSP_TYPE Type)
{
    unsigned index = (unsigned)~0;

    // Make sure it is in range
    assert(FINESSE_FUSE_RSP_NONE <= Type);
    assert(FINESSE_FUSE_RSP_MAX > Type);

    index = Type - FINESSE_FUSE_RSP_NONE;
    __sync_fetch_and_add(&FinesseServerStats->FuseResponses[index], 1);
}
