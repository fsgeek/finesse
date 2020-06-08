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

// I'm concerned about leaking these
uint64_t finesse_alloc_count;
uint64_t finesse_free_count;

struct fuse_req *FinesseAllocFuseRequest(struct fuse_session *se)
{
    struct finesse_req *freq;

    assert(NULL != se);

    freq = (struct finesse_req *)calloc(1, sizeof(struct finesse_req));
    if (freq == NULL)
    {
        fprintf(stderr, "finesse (fuse): failed to allocate request\n");
    }
    else
    {
        freq->fuse_request.se = se;
        freq->fuse_request.ctr = 1;
        list_init_req(&freq->fuse_request);
        pthread_mutex_init(&freq->fuse_request.lock, NULL);
        finesse_set_provider(&freq->fuse_request, 1);
        pthread_mutex_init(&freq->lock, NULL);
        pthread_cond_init(&freq->condition, NULL);
    }
    finesse_alloc_count++;
    return &freq->fuse_request;
}

void FinesseDestroyFuseRequest(fuse_req_t req)
{
    struct finesse_req *freq = (struct finesse_req *)req;

    assert(NULL != freq);
    assert(0 == req->ctr);
    // Clean up captured iovec data
    if (NULL != freq->iov) {
        for (unsigned index = 0; index < freq->iov_count; index++) {
            if (NULL != freq->iov[index].iov_base) {
                free(freq->iov[index].iov_base);
                freq->iov[index].iov_base = NULL;
            }
        }
        free(freq->iov);
        freq->iov = NULL;
    }
    pthread_mutex_destroy(&req->lock);
    pthread_cond_destroy(&freq->condition);
    pthread_mutex_destroy(&freq->lock);
    memset(req, 0, sizeof(struct finesse_req));
    free(req);
    finesse_free_count++;
}

void FinesseFreeFuseRequest(fuse_req_t req)
{
    int ctr;
    struct fuse_session *se = req->se;

    assert(NULL != se);

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

