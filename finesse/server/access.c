/*
  Copyright (C) 2020  Tony Mason <fsgeek@cs.ubc.ca>
*/
#include "fs-internal.h"

static int Access(struct fuse_session *se, void *Client, fincomm_message Message)
{
    finesse_msg *            fmsg   = NULL;
    int                      status = 0;
    finesse_object_t *       finobj = NULL;
    finesse_server_handle_t *fsh;
    fuse_ino_t               parent          = FUSE_ROOT_ID;
    struct fuse_req *        fuse_request    = NULL;
    struct finesse_req *     finesse_request = NULL;
    struct fuse_out_header * out             = NULL;

    assert(NULL != se);
    assert(NULL != Client);
    assert(NULL != Message);

    while (1) {
        // fmsg->Message.Fuse.Request.Parameters.Access.ParentInode;
        // fmsg->Message.Fuse.Request.Parameters.Access.Mask;
        // fmsg->Message.Fuse.Request.Parameters.Access.Name;
        fsh  = (finesse_server_handle_t)se->server_handle;
        fmsg = (finesse_msg *)Message->Data;

        if (uuid_is_null(fmsg->Message.Fuse.Request.Parameters.Access.ParentInode)) {
            parent = FUSE_ROOT_ID;
        }
        else {
            finobj = finesse_object_lookup_by_uuid(&fmsg->Message.Fuse.Request.Parameters.Access.ParentInode);
            if (NULL == finobj) {
                //
                status = FinesseSendAccessResponse(fsh, Client, Message, EBADF);
                assert(0 == status);
                FinesseCountFuseResponse(FINESSE_FUSE_RSP_ERR);
                status = 0;
                break;
            }
        }

        // We need to ask the file system, which means we need a request.
        fuse_request = FinesseAllocFuseRequest(se);

        if (NULL == fuse_request) {
            fprintf(stderr, "%s @ %d (%s): alloc failure\n", __FILE__, __LINE__, __func__);
            // TODO: fix this function's prototype

            status = FinesseSendAccessResponse(fsh, Client, Message, ENOMEM);

            if (0 == status) {
                FinesseCountFuseResponse(FINESSE_FUSE_RSP_ERR);
            }
            break;
        }

        fuse_request->ctr++;  // make sure it doesn't go away until we're done processing it.
        finesse_request      = (struct finesse_req *)fuse_request;
        fuse_request->opcode = FUSE_ACCESS;
        finesse_original_ops->access(fuse_request, parent, fmsg->Message.Fuse.Request.Parameters.Access.Mask);

        FinesseWaitForFuseRequestCompletion(finesse_request);

        assert(finesse_request->iov_count > 0);
        out = finesse_request->iov[0].iov_base;

        status = FinesseSendStatfsResponse(fsh, Client, Message, NULL, out->error);
        assert(0 == status);
        FinesseCountFuseResponse(FINESSE_FUSE_RSP_STATFS);
        FinesseFreeFuseRequest(fuse_request);
        status = 0;
        break;
    }

    if (NULL != finobj) {
        finesse_object_release(finobj);
        finobj = NULL;
    }

    return status;
}

FinesseServerFunctionHandler FinesseServerFuseAccess = Access;
