/*
  Copyright (C) 2020  Tony Mason <fsgeek@cs.ubc.ca>
*/
#include "fs-internal.h"


static void list_init_req(struct fuse_req *req)
{
    req->next = req;
    req->prev = req;
}

static void list_del_req(struct fuse_req *req)
{
    struct fuse_req *prev = req->prev;
    struct fuse_req *next = req->next;
    prev->next = next;
    next->prev = prev;
}

struct fuse_req *FinesseAllocFuseRequest(struct fuse_session *se)
{
    struct fuse_req *req;
    struct finesse_req *freq;

    req = (struct fuse_req *)calloc(1, sizeof(struct finesse_req));
    if (req == NULL)
    {
        fprintf(stderr, "finesse (fuse): failed to allocate request\n");
    }
    else
    {
        req->se = se;
        req->ctr = 1;
        list_init_req(req);
        pthread_mutex_init(&req->lock, NULL);
        finesse_set_provider(req, 1);
        freq = (struct finesse_req *)req;
        pthread_mutex_init(&freq->lock, NULL);
        pthread_cond_init(&freq->condition, NULL);
    }
    return req;
}

static void FinesseDestroyFuseRequest(fuse_req_t req)
{
    struct finesse_req *freq = (struct finesse_req *)req;

    assert(NULL != freq);

    pthread_mutex_destroy(&req->lock);
    pthread_cond_destroy(&freq->condition);
    pthread_mutex_destroy(&freq->lock);
    free(req);
}

void FinesseFreeFuseRequest(fuse_req_t req)
{
    int ctr;
    struct fuse_session *se = req->se;

    pthread_mutex_lock(&se->lock);
    req->u.ni.func = NULL;
    req->u.ni.data = NULL;
    list_del_req(req);
    ctr = --req->ctr;
    fuse_chan_put(req->ch);
    req->ch = NULL;
    pthread_mutex_unlock(&se->lock);
    if (!ctr) {
        FinesseDestroyFuseRequest(req);
    }
}

