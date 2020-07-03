/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

#include <fincomm.h>

struct finesse_req
{
    struct fuse_req       fuse_request;

    /* finesse specific routing information */
    int                    status;
    struct iovec *         iov;
    int                    iov_count;
    pthread_mutex_t        lock;
    pthread_cond_t         condition; // use to signal the waiting thread
    int                    completed;
    struct fuse_attr       attr;
    struct fuse_out_header out;
};

extern const struct fuse_lowlevel_ops *finesse_original_ops;

struct fuse_req *FinesseAllocFuseRequest(struct fuse_session *se);
void FinesseFreeFuseRequest(fuse_req_t req);
void FinesseDestroyFuseRequest(fuse_req_t req);
void FinesseReleaseInode(struct fuse_session *se, fuse_ino_t ino);

typedef int (*FinesseServerFunctionHandler)(struct fuse_session *se, void *Client, fincomm_message Message);

void FinesseServerFuseStatFs(struct fuse_session *se, void *Client, fincomm_message Message);
void FinesseSignalFuseRequestCompletion(struct finesse_req *req);
void FinesseWaitForFuseRequestCompletion(struct finesse_req *req);

int FinesseServerHandleNativeRequest(struct fuse_session *se, void *Client, fincomm_message Message);
int FinesseServerHandleFuseRequest(struct fuse_session *se, void *Client, fincomm_message Message);

int FinesseServerNativeTestRequest(finesse_server_handle_t Fsh, void *Client, fincomm_message Message);
int FinesseServerNativeMapRequest(struct fuse_session *se, void *Client, fincomm_message Message);

int FinesseServerNativeMapReleaseRequest(finesse_server_handle_t Fsh, void *Client, fincomm_message Message);

int FinesseServerNativeServerStatRequest(finesse_server_handle_t Fsh, void *Client, fincomm_message Message);

FinesseServerFunctionHandler FinesseServerFuseStat;
FinesseServerFunctionHandler FinesseServerFuseAccess;
