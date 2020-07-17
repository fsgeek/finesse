/*
  Copyright (C) 2020  Tony Mason <fsgeek@cs.ubc.ca>
*/
#include "fs-internal.h"

static int Unlink(struct fuse_session *se, void *Client, fincomm_message Message)
{
    finesse_msg *            fmsg   = NULL;
    int                      status = 0;
    finesse_server_handle_t *fsh;
    finesse_object_t *       finobj          = NULL;
    struct fuse_req *        fuse_request    = NULL;
    struct finesse_req *     finesse_request = NULL;
    struct fuse_out_header * out             = NULL;

    assert(NULL != se);
    assert(NULL != Client);
    assert(NULL != Message);

    fsh  = (finesse_server_handle_t)se->server_handle;
    fmsg = (finesse_msg *)Message->Data;

    while (1) {
        finobj = finesse_object_lookup_by_uuid(&fmsg->Message.Fuse.Request.Parameters.Unlink.Parent);

        if (NULL == finobj) {
            // Didn't find it
            status = FinesseSendUnlinkResponse(fsh, Client, Message, EBADF);
            assert(0 == status);
            break;
        }

        // fmsg->Message.Fuse.Request.Parameters.Unlink.Name;

        // We need to getattr at this point
        fuse_request    = FinesseAllocFuseRequest(se);
        finesse_request = (struct finesse_req *)fuse_request;
        fuse_request->ctr++;  // ensure's it doesn't go away before we're done with it
        fuse_request->opcode = FUSE_UNLINK;
        finesse_original_ops->unlink(fuse_request, finobj->inode, fmsg->Message.Fuse.Request.Parameters.Unlink.Name);

        FinesseWaitForFuseRequestCompletion(finesse_request);

        assert(finesse_request->iov_count > 0);
        out = finesse_request->iov[0].iov_base;

        status = FinesseSendUnlinkResponse(fsh, Client, Message, out->error);
        assert(0 == status);

        break;
    }

    if (NULL != finobj) {
        finesse_object_release(finobj);
        finobj = NULL;
    }

    FinesseCountFuseResponse(FINESSE_FUSE_RSP_ATTR);

    if (NULL != fuse_request) {
        FinesseFreeFuseRequest(fuse_request);
    }

    return status;
}

FinesseServerFunctionHandler FinesseServerFuseUnlink = Unlink;
