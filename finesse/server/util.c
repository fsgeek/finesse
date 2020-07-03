/*
  Copyright (C) 2020  Tony Mason <fsgeek@cs.ubc.ca>
*/
#include "fs-internal.h"

void FinesseWaitForFuseRequestCompletion(struct finesse_req *req)
{
    assert(NULL != req);
    assert(req->fuse_request.finesse.allocated);  // otherwise this shouldn't be passed here!
    assert(NULL != req->fuse_request.se);

    if (0 == req->completed) {
        pthread_mutex_lock(&req->lock);
        while (0 == req->completed) {
            pthread_cond_wait(&req->condition, &req->lock);
        }
        pthread_mutex_unlock(&req->lock);
    }
}

void FinesseSignalFuseRequestCompletion(struct finesse_req *req)
{
    assert(NULL != req);
    assert(req->fuse_request.finesse.allocated);  // otherwise this shouldn't be passed here!
    assert(NULL != req->fuse_request.se);

    assert(0 == req->completed);

    // No lock needed
    req->completed = 1;
    pthread_cond_broadcast(&req->condition);
}

void FinesseReleaseInode(struct fuse_session *se, fuse_ino_t ino)
{
    struct fuse_req *fuse_request = FinesseAllocFuseRequest(se);

    fuse_request->opcode = FUSE_FORGET;
    finesse_original_ops->forget(fuse_request, ino, 1);
}
