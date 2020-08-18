//
// (C) Copyright 2020 Tony Mason
//
#include "fs-internal.h"

static const char *finesse_request_type_to_string(FINESSE_FUSE_REQ_TYPE Type)
{
    const char *str = "UNKNOWN REQUEST TYPE";

    switch (Type) {
        case FINESSE_FUSE_REQ_STATFS:
            str = "statfs";
            break;
        case FINESSE_FUSE_REQ_LOOKUP:
            str = "lookup";
            break;
        case FINESSE_FUSE_REQ_FORGET:
            str = "forget";
            break;
        case FINESSE_FUSE_REQ_STAT:
            str = "stat";
            break;
        case FINESSE_FUSE_REQ_GETATTR:
            str = "getattr";
            break;
        case FINESSE_FUSE_REQ_SETATTR:
            str = "setattr";
            break;
        case FINESSE_FUSE_REQ_READLINK:
            str = "readlink";
            break;
        case FINESSE_FUSE_REQ_MKNOD:
            str = "mknod";
            break;
        case FINESSE_FUSE_REQ_MKDIR:
            str = "mkdir";
            break;
        case FINESSE_FUSE_REQ_UNLINK:
            str = "unlink";
            break;
        case FINESSE_FUSE_REQ_RMDIR:
            str = "rmdir";
            break;
        case FINESSE_FUSE_REQ_SYMLINK:
            str = "symlink";
            break;
        case FINESSE_FUSE_REQ_RENAME:
            str = "rename";
            break;
        case FINESSE_FUSE_REQ_LINK:
            str = "link";
            break;
        case FINESSE_FUSE_REQ_OPEN:
            str = "open";
            break;
        case FINESSE_FUSE_REQ_READ:
            str = "read";
            break;
        case FINESSE_FUSE_REQ_WRITE:
            str = "write";
            break;
        case FINESSE_FUSE_REQ_FLUSH:
            str = "flush";
            break;
        case FINESSE_FUSE_REQ_RELEASE:
            str = "release";
            break;
        case FINESSE_FUSE_REQ_FSYNC:
            str = "fsync";
            break;
        case FINESSE_FUSE_REQ_OPENDIR:
            str = "opendir";
            break;
        case FINESSE_FUSE_REQ_READDIR:
            str = "readdir";
            break;
        case FINESSE_FUSE_REQ_RELEASEDIR:
            str = "releasedir";
            break;
        case FINESSE_FUSE_REQ_FSYNCDIR:
            str = "fsyncdir";
            break;
        case FINESSE_FUSE_REQ_SETXATTR:
            str = "setxattr";
            break;
        case FINESSE_FUSE_REQ_GETXATTR:
            str = "getxattr";
            break;
        case FINESSE_FUSE_REQ_LISTXATTR:
            str = "listxattr";
            break;
        case FINESSE_FUSE_REQ_REMOVEXATTR:
            str = "removexattr";
            break;
        case FINESSE_FUSE_REQ_ACCESS:
            str = "access";
            break;
        case FINESSE_FUSE_REQ_CREATE:
            str = "create";
            break;
        case FINESSE_FUSE_REQ_GETLK:
            str = "getlk";
            break;
        case FINESSE_FUSE_REQ_SETLK:
            str = "setlk";
            break;
        case FINESSE_FUSE_REQ_BMAP:
            str = "bmap";
            break;
        case FINESSE_FUSE_REQ_IOCTL:
            str = "ioctl";
            break;
        case FINESSE_FUSE_REQ_POLL:
            str = "poll";
            break;
        case FINESSE_FUSE_REQ_WRITE_BUF:
            str = "write buf";
            break;
        case FINESSE_FUSE_REQ_RETRIEVE_REPLY:
            str = "retrieve reply";
            break;
        case FINESSE_FUSE_REQ_FORGET_MULTI:
            str = "forget multi";
            break;
        case FINESSE_FUSE_REQ_FLOCK:
            str = "flock";
            break;
        case FINESSE_FUSE_REQ_FALLOCATE:
            str = "fallocate";
            break;
        case FINESSE_FUSE_REQ_READDIRPLUS:
            str = "readdirplus";
            break;
        case FINESSE_FUSE_REQ_COPY_FILE_RANGE:
            str = "copy file range";
            break;
        case FINESSE_FUSE_REQ_LSEEK:
            str = "lseek";
            break;
        default:
            break;  // just use the default string type
    }

    return str;
}

int FinesseServerHandleFuseRequest(struct fuse_session *se, void *Client, fincomm_message Message)
{
    finesse_server_handle_t fsh  = (finesse_server_handle_t)se->server_handle;
    finesse_msg *           fmsg = NULL;

    if (NULL == fsh) {
        return ENOTCONN;
    }

    assert(FINESSE_REQUEST == Message->MessageType);  // nothing else makes sense here
    assert(NULL != Message);
    fmsg = (finesse_msg *)Message->Data;
    assert(NULL != fmsg);
    assert(FINESSE_FUSE_MESSAGE == fmsg->MessageClass);

    fuse_log(FUSE_LOG_INFO, "FINESSE %s: FUSE request (0x%p) type %d (%s)\n", __func__, fmsg, fmsg->Message.Fuse.Request.Type,
             finesse_request_type_to_string(fmsg->Message.Fuse.Request.Type));

    FinesseCountFuseRequest(fmsg->Message.Fuse.Request.Type);

    // Now the big long switch statement
    switch (fmsg->Message.Fuse.Request.Type) {
        case FINESSE_FUSE_REQ_STATFS:
            FinesseServerFuseStatFs(se, Client, Message);
            break;

        case FINESSE_FUSE_REQ_STAT:
            FinesseServerFuseStat(se, Client, Message);
            break;

        case FINESSE_FUSE_REQ_ACCESS:
            FinesseServerFuseAccess(se, Client, Message);
            break;

        case FINESSE_FUSE_REQ_UNLINK:
            FinesseServerFuseUnlink(se, Client, Message);
            break;

        case FINESSE_FUSE_REQ_LOOKUP:
        case FINESSE_FUSE_REQ_FORGET:
        case FINESSE_FUSE_REQ_GETATTR:
        case FINESSE_FUSE_REQ_SETATTR:
        case FINESSE_FUSE_REQ_READLINK:
        case FINESSE_FUSE_REQ_MKNOD:
        case FINESSE_FUSE_REQ_MKDIR:
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
            fuse_log(FUSE_LOG_ERR, "FINESSE %s: FUSE request (0x%p) returning ENOTSUP\n", __func__, fmsg);
            fmsg->Message.Fuse.Response.Type = FINESSE_FUSE_RSP_ERR;
            fmsg->Result                     = ENOTSUP;
            FinesseSendResponse(fsh, Client, Message);
            break;
    }

    return 0;
}
