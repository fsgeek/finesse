/*
  Copyright (C) 2020  Tony Mason <fsgeek@cs.ubc.ca>
*/
#include "config.h"
#include "fuse_i.h"
#include "fuse_kernel.h"
#include "fuse_opt.h"
#include "fuse_misc.h"
#include <fuse_lowlevel.h>
#include "finesse-fuse.h"
#include <finesse-server.h>

#if !defined(VARIABLE_IS_NOT_USED)
#ifdef __GNUC__
#define VARIABLE_IS_NOT_USED __attribute__ ((unused))
#else
#define VARIABLE_IS_NOT_USED
#endif
#endif // VARIABLE_IS_NOT_USED

typedef struct finesse_lookup_info
{
    int status;

    /* used so the caller can wait for completion */
    pthread_mutex_t lock;
    pthread_cond_t condition;
    int completed;

    /* attributes */
    struct fuse_attr attr;

} finesse_lookup_info_t;


int FinesseServerInternalNameMapRequest(struct fuse_session *se, uuid_t *Parent, const char *Name, finesse_object_t **Finobj);


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
