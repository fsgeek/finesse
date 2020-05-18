//
// (C) Copyright 2020 Tony Mason
//
#include "fs-internal.h"


int FinesseServerHandleFuseRequest(struct fuse_session *se, void *Client, fincomm_message Message)
{
    finesse_server_handle_t fsh = (finesse_server_handle_t)se->server_handle;
    finesse_msg *fmsg = NULL;

    if (NULL == fsh) {
        return ENOTCONN;
    }

    assert(FINESSE_REQUEST == Message->MessageType); // nothing else makes sense here
    assert(NULL != Message);
    fmsg = (finesse_msg *)Message->Data;
    assert(NULL != fmsg);
    assert(FINESSE_FUSE_MESSAGE == fmsg->MessageClass);

    // Now the big long switch statement
    switch (fmsg->Message.Fuse.Request.Type) {
        case FINESSE_FUSE_REQ_STATFS:
        FinesseServerFuseStatFs(se, Client, Message);
        break;

        case FINESSE_FUSE_REQ_LOOKUP:
        case FINESSE_FUSE_REQ_FORGET:
        case FINESSE_FUSE_REQ_GETATTR:
        case FINESSE_FUSE_REQ_SETATTR:
        case FINESSE_FUSE_REQ_READLINK:
        case FINESSE_FUSE_REQ_MKNOD:
        case FINESSE_FUSE_REQ_MKDIR:
        case FINESSE_FUSE_REQ_UNLINK:
        case FINESSE_FUSE_REQ_RMDIR:
        case FINESSE_FUSE_REQ_SYMLINK:
        case FINESSE_FUSE_REQ_RENAME:
        case FINESSE_FUSE_REQ_LINK:
        case FINESSE_FUSE_REQ_OPEN:
        case FINESSE_FUSE_REQ_READ:
        case FINESSE_FUSE_REQ_WRITE:
        case FINESSE_FUSE_REQ_FLUSH:
        case FINESSE_FUSE_REQ_RELEASE:
        case FINESSE_FUSE_REQ_FSYNC:
        case FINESSE_FUSE_REQ_OPENDIR:
        case FINESSE_FUSE_REQ_READDIR:
        case FINESSE_FUSE_REQ_RELEASEDIR:
        case FINESSE_FUSE_REQ_FSYNCDIR:
        case FINESSE_FUSE_REQ_SETXATTR:
        case FINESSE_FUSE_REQ_GETXATTR:
        case FINESSE_FUSE_REQ_LISTXATTR:
        case FINESSE_FUSE_REQ_REMOVEXATTR:
        case FINESSE_FUSE_REQ_ACCESS:
        case FINESSE_FUSE_REQ_CREATE:
        case FINESSE_FUSE_REQ_GETLK:
        case FINESSE_FUSE_REQ_SETLK:
        case FINESSE_FUSE_REQ_BMAP:
        case FINESSE_FUSE_REQ_IOCTL:
        case FINESSE_FUSE_REQ_POLL:
        case FINESSE_FUSE_REQ_WRITE_BUF:
        case FINESSE_FUSE_REQ_RETRIEVE_REPLY:
        case FINESSE_FUSE_REQ_FORGET_MULTI:
        case FINESSE_FUSE_REQ_FLOCK:
        case FINESSE_FUSE_REQ_FALLOCATE:
        case FINESSE_FUSE_REQ_READDIRPLUS:
        case FINESSE_FUSE_REQ_COPY_FILE_RANGE:
        case FINESSE_FUSE_REQ_LSEEK:
        default:
            fuse_log(FUSE_LOG_ERR, "Finesse %s: unsupported request %d, returning ENOTSUP", __PRETTY_FUNCTION__, fmsg->Message.Fuse.Request.Type);
            fmsg->Message.Fuse.Response.Type = FINESSE_FUSE_RSP_ERR;
            fmsg->Message.Fuse.Response.Parameters.ReplyErr.Err = ENOTSUP;
            FinesseSendResponse(fsh, Client, Message);
            break;
    }

    return 0;
}
