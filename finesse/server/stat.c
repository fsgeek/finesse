/*
  Copyright (C) 2020  Tony Mason <fsgeek@cs.ubc.ca>
*/
#include "fs-internal.h"

static int Stat(struct fuse_session *se, void *Client, fincomm_message Message)
{
    finesse_msg *fmsg = (finesse_msg *)Message->Data;
    int status = 0;
    finesse_server_handle_t *fsh;
    fuse_ino_t parent = FUSE_ROOT_ID;
    fuse_ino_t ino = 0;
    struct fuse_req *fuse_request = NULL;
    struct finesse_req *finesse_request = NULL;
    struct fuse_out_header *out = NULL;

    assert(NULL != se);
    assert(NULL != Client);
    assert(NULL != Message);

    fsh = (finesse_server_handle_t)se->server_handle;
    fmsg = (finesse_msg *)Message->Data;

    // fmsg->Message.Fuse.Request.Parameters.Stat.ParentInode;
    // fmsg->Message.Fuse.Request.Parameters.Stat.Inode;
    // fmsg->Message.Fuse.Request.Parameters.Stat.Flags;
    // fmsg->Message.Fuse.Request.Parameters.Stat.Name;

    // There are FOUR possible paths right now for this:
    // Stat: this is a path
    // Fstat: this is a parent inode + path
    // Lstat: this is a path but doesn't follow the symlink
    // Fstatat: this is a parent inode + path and may (or not) follow the symlink, as specified by flags
    // Note: fstatat also has a "don't automount" flag, but I don't think we can do anything about that one.
    //

    parent = finesse_lookup_ino(&fmsg->Message.Fuse.Request.Parameters.Stat.ParentInode);
    ino = finesse_lookup_ino(&fmsg->Message.Fuse.Request.Parameters.Stat.Inode);

    if ((0 == parent) || (0 == ino)) {
        status = FinesseSendAccessResponse(fsh, Client, Message, EBADF);
        assert(0 == status);
        FinesseCountFuseResponse(FINESSE_FUSE_RSP_ERR);
        return 0;
    }

    // stat: parent = ROOT, ino = ROOT, flags = 0
    // fstat: parent = ROOT, ino = some value other than 0 or FUSE_ROOT_ID, flags = 0
    // lstat: parent = ROOT, ino = ROOT, flags = don't follow link
    // fstatat: parent = ROOT, ino = Root

    fuse_request = FinesseAllocFuseRequest(se);

    if (NULL == fuse_request) {
        fprintf(stderr, "%s @ %d (%s): alloc failure\n", __FILE__, __LINE__, __FUNCTION__);
        // TODO: fix this function's prototype

        status = FinesseSendAccessResponse(fsh, Client, Message, ENOMEM);

        if (0 == status) {
          FinesseCountFuseResponse(FINESSE_FUSE_RSP_ERR);
        }

        return status;
    }

    fuse_request->ctr++; // make sure it doesn't go away until we're done processing it.
    finesse_request = (struct finesse_req *)fuse_request;
    fuse_request->opcode = FUSE_ACCESS;
    finesse_original_ops->access(fuse_request, parent, fmsg->Message.Fuse.Request.Parameters.Access.Mask);

    FinesseWaitForFuseRequestCompletion(finesse_request);

    assert(finesse_request->iov_count > 0);
    out = finesse_request->iov[0].iov_base;

    status = FinesseSendStatfsResponse(fsh, Client, Message, NULL, out->error);
    assert(0 == status);
    FinesseCountFuseResponse(FINESSE_FUSE_RSP_STATFS);
    FinesseFreeFuseRequest(fuse_request);

    return 0;

}

FinesseServerFunctionHandler FinesseServerFuseStat = Stat;

