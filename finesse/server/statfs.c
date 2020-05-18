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
        status = FinesseSendFstatfsResponse(fsh, Client, Message, NULL, EBADF);
        assert(0 == status);
        return 0;
    }

    // We need to ask the file system, which means we need a request.
    fuse_request = FinesseAllocFuseRequest(se);

    if (NULL == fuse_request) {
        fprintf(stderr, "%s @ %d (%s): alloc failure\n", __FILE__, __LINE__, __FUNCTION__);
        // TODO: fix this function's prototype

        return FinesseSendFstatfsResponse(fsh, Client, Message,NULL, ENOMEM);
    }

    fuse_request->ctr++; // make sure it doesn't go away until we're done processing it.
    finesse_request = (struct finesse_req *)fuse_request;
    fuse_request->opcode = FUSE_STATFS;
    finesse_original_ops->statfs(fuse_request, finobj->inode);

    FinesseWaitForFuseRequestCompletion(finesse_request);

    assert(finesse_request->iov_count > 0);
    out = finesse_request->iov[0].iov_base;

    // If it is an error, or we didn't get data back
    if ((0 != out->error) || (finesse_request->iov_count < 2) || (finesse_request->iov[1].iov_len < sizeof(struct statvfs))){
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
    FinesseFreeFuseRequest(fuse_request); // drops that extra reference we added above.
    fuse_request = NULL;
    finesse_request = NULL;

    return 0;
}

static int finesse_statfs_handler(struct fuse_session *se, void *Client, fincomm_message Message, const char *PathName)
{
    finesse_server_handle_t *fsh;
    finesse_object_t *finobj = NULL;
    int status;
    struct fuse_req *fuse_request;
    struct fuse_out_header *out = NULL;
    struct statvfs *arg = NULL;
    struct finesse_req *finesse_request;
    finesse_msg *fmsg;
    size_t len;
    
    assert(NULL != se);
    assert(NULL != Client);
    assert(NULL != Message);
    assert(NULL != PathName);

    fmsg = (finesse_msg *)Message->Data;
    fsh = (finesse_server_handle_t)se->server_handle;

    // We need to ask the file system, which means we need a request.
    fuse_request = FinesseAllocFuseRequest(se);
    fuse_request->ctr++; // make sure it doesn't go away while we need it.
    finesse_request = (struct finesse_req *)fuse_request;

    if (NULL == fuse_request) {
        fprintf(stderr, "%s @ %d (%s): alloc failure\n", __FILE__, __LINE__, __FUNCTION__);
        // TODO: fix this function's prototype

        return FinesseSendStatfsResponse(fsh, Client, Message,NULL, ENOMEM);
    }

    while (NULL != fuse_request) {
        // We need to do a lookup here
        status = FinesseServerInternalNameMapRequest(se, fmsg->Message.Fuse.Request.Parameters.Statfs.Options.Name, &finobj);

        if (0 != status) {
            assert(NULL == finobj);
            status = FinesseSendStatfsResponse(fsh, Client, Message, NULL, ENOENT);
            break;
        }

        assert(NULL != finobj); // that wouldn't make sense
        finesse_request->completed = 0;
        fuse_request->ctr++; // we want to hold on to this until we are done with it
        fuse_request->opcode = FUSE_STATFS; // Fuse internal call
        finesse_original_ops->statfs(fuse_request, finobj->inode);

        FinesseWaitForFuseRequestCompletion(finesse_request);
        assert(NULL != fuse_request->se);

        if (finesse_request->iov_count != 0) {
            status = FinesseSendStatfsResponse(fsh, Client, Message, NULL, ENOMEM);
            break;
        }

        out = finesse_request->iov[0].iov_base;

        if ((finesse_request->iov_count == 1) || (0 != out->error)) {
            status = FinesseSendStatfsResponse(fsh, Client, Message, NULL, out->error);
            break;
        }

        arg = finesse_request->iov[1].iov_base;
        len = finesse_request->iov[1].iov_len;

        if (len < sizeof(struct statvfs)) {
            status = FinesseSendStatfsResponse(fsh, Client, Message, NULL, EOVERFLOW);
            break;
        }

        status = FinesseSendStatfsResponse(fsh, Client, Message, arg, 0);
        break; // done
    }

    // cleanup
    if (NULL != finobj) {
        finesse_object_release(finobj);
        finobj = NULL;
    }

    if (NULL != fuse_request) {
        FinesseFreeFuseRequest(fuse_request);
        fuse_request = NULL;
        finesse_request = NULL;
    }

    return 0;
}



void FinesseServerFuseStatFs(struct fuse_session *se, void *Client, fincomm_message Message)
{
    finesse_msg *fmsg = (finesse_msg *)Message->Data;
    int status;

    (void) se;
    (void) Client;

    if (STATFS == fmsg->Message.Fuse.Request.Parameters.Statfs.StatFsType) {
        status = finesse_statfs_handler(se, Client, Message, fmsg->Message.Fuse.Request.Parameters.Statfs.Options.Name);
        assert(0 == status);
    }
    else {
        assert(FSTATFS == fmsg->Message.Fuse.Request.Parameters.Statfs.StatFsType);
        status = finesse_fstatfs_handler(se, Client, Message, &fmsg->Message.Fuse.Request.Parameters.Statfs.Options.Inode);
        assert(0 == status);
    }
}
