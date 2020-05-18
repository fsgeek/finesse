/*
  Copyright (C) 2020  Tony Mason <fsgeek@cs.ubc.ca>
*/
#include "fs-internal.h"

// do a synchronous lookup
void FinesseServerInternalLookup(struct fuse_session *se, const char *Path)
{
    (void) se;
    (void) Path;
    assert(0); // TODO
}

void FinesseWaitForFuseRequestCompletion(struct finesse_req *req)
{
    assert(NULL != req);
    assert(req->fuse_request.finesse.allocated); // otherwise this shouldn't be passed here!

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
    assert(req->fuse_request.finesse.allocated); // otherwise this shouldn't be passed here!

    assert(0 == req->completed);

    // No lock needed
    req->completed = 1;
    pthread_cond_broadcast(&req->condition);
}