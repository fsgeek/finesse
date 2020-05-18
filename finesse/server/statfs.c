/*
  Copyright (C) 2020  Tony Mason <fsgeek@cs.ubc.ca>
*/
#include "fs-internal.h"


static int finesse_fstatfs_handler(struct fuse_session *se, void *Client, fincomm_message Message, uuid_t *key)
{
    finesse_server_handle_t *fsh;
    finesse_object_t *finobj = NULL;
    int status;
    struct fuse_req *fuse_request;
    struct fuse_out_header *out = NULL;
    struct statvfs *arg = NULL;
    struct finesse_req *finesse_request;
    

    assert(NULL != se);
    assert(NULL != Client);
    assert(NULL != Message);
    assert(NULL != key);
    assert(!uuid_is_null(*key));
    fsh = (finesse_server_handle_t)se->server_handle;

    finobj =  finesse_object_lookup_by_uuid(key);
    if (NULL == finobj) {
        // bad handle
        status = FinesseSendNameMapResponse(fsh, Client, Message, key, EBADF);
        assert(0 == status);
        return 0;
    }

    // We need to ask the file system, which means we need a request.
    fuse_request = FinesseAllocFuseRequest(se);
    finesse_request = (struct finesse_req *)fuse_request;
    assert(NULL != fuse_request);

    if (NULL == fuse_request) {
        fprintf(stderr, "%s @ %d (%s): alloc failure\n", __FILE__, __LINE__, __FUNCTION__);
        // TODO: fix this function's prototype
        return FinesseSendNameMapResponse(fsh, Client, Message, key, ENOMEM);      
    }

    fuse_request->finesse.message = Message;
    fuse_request->finesse.client = Client;
    fuse_request->opcode = FUSE_STATFS;
    fuse_request->se = se;
    finesse_original_ops->statfs(fuse_request, finobj->inode);

    FinesseWaitForFuseRequestCompletion(finesse_request);

    assert(finesse_request->iov_count > 0);
    out = finesse_request->iov[0].iov_base;

    if (finesse_request->iov_count < 2) {
        // I'm assuming this is an error state?
        assert(0 != out);
        status = FinesseSendStatfsResponse(fsh, Client, Message, NULL, out->error);
        assert(0 == status);
        FinesseFreeFuseRequest(fuse_request);
        return 0;
    }

    arg = finesse_request->iov[1].iov_base;
    assert(finesse_request->iov[1].iov_len >= sizeof(struct statvfs));

    status = FinesseSendStatfsResponse(fsh, Client, Message, arg, 0);
    assert (0 == status);

    // clean up
    finesse_object_release(finobj);
    finobj = NULL;
    fuse_request->finesse.client = NULL; // message is gone
    FinesseFreeFuseRequest(fuse_request);
    fuse_request = NULL;
    finesse_request = NULL;

    return 0;
}


void FinesseServerFuseStatFs(struct fuse_session *se, void *Client, fincomm_message Message)
{
    finesse_msg *fmsg = (finesse_msg *)Message->Data;
    int status;

    (void) se;
    (void) Client;

    if (STATFS == fmsg->Message.Fuse.Request.Parameters.Statfs.StatFsType) {
        assert(0);
    }
    else {
        assert(FSTATFS == fmsg->Message.Fuse.Request.Parameters.Statfs.StatFsType);
        status = finesse_fstatfs_handler(se, Client, Message, &fmsg->Message.Fuse.Request.Parameters.Statfs.Options.Inode);
        assert(status);
    }
}
