/*
  Copyright (C) 2020  Tony Mason <fsgeek@cs.ubc.ca>
*/
#include "fs-internal.h"

static int Stat(struct fuse_session *se, void *Client, fincomm_message Message) {
    finesse_msg *            fmsg   = NULL;
    int                      status = 0;
    finesse_server_handle_t *fsh;
    finesse_object_t *       parentobj       = NULL;
    fuse_ino_t               parentino       = 0;
    finesse_object_t *       finobj          = NULL;
    fuse_ino_t               ino             = 0;
    struct fuse_req *        fuse_request    = NULL;
    struct finesse_req *     finesse_request = NULL;
    struct fuse_out_header * out             = NULL;
    static const struct stat zerostat;
    struct stat              statout;
    struct fuse_attr_out *   arg;
    double                   timeout;
    char                     uuid_buffer[40];
    const char *             Name = uuid_buffer;

    assert(NULL != se);
    assert(NULL != Client);
    assert(NULL != Message);

    fsh  = (finesse_server_handle_t)se->server_handle;
    fmsg = (finesse_msg *)Message->Data;

    while (1) {
        // fmsg->Message.Fuse.Request.Parameters.Stat.ParentInode;
        // fmsg->Message.Fuse.Request.Parameters.Stat.Inode;
        // fmsg->Message.Fuse.Request.Parameters.Stat.Flags;
        // fmsg->Message.Fuse.Request.Parameters.Stat.Name;

        // There are FOUR possible paths right now for this:
        // Stat: this is a path (path relative to root)
        // Fstat: this is a parent inode + path (path relative to parent)
        //
        // Lstat: this is a path but doesn't follow the symlink
        // Fstatat: this is a parent inode + path and may (or not) follow the
        // symlink, as specified by flags Note: fstatat also has a "don't automount"
        // flag, but I don't think we can do anything about that one.
        //

        parentino = FUSE_ROOT_ID;
        if (!uuid_is_null(fmsg->Message.Fuse.Request.Parameters.Stat.ParentInode)) {
            finobj = finesse_object_lookup_by_uuid(&fmsg->Message.Fuse.Request.Parameters.Stat.ParentInode);

            if (NULL == finobj) {
                // invalid parent uuid
                status = FinesseSendStatResponse(fsh, Client, Message, &zerostat, 0, EBADF);
                assert(0 == status);
                break;
            }

            parentino = finobj->inode;
            finesse_object_release(finobj);
            finobj = NULL;
        }

        if (uuid_is_null(fmsg->Message.Fuse.Request.Parameters.Stat.Inode)) {
            // This must be name based, so let's go get the inode; we're skipping inserting it into
            // the inode tracking list.
            Name   = &fmsg->Message.Fuse.Request.Parameters.Stat.Name[0];
            status = FinsesseServerInternalNameLookup(se, parentino, Name, &ino);
            if (0 != status) {
                break;
            }
        } else {
            // This is UUID based; note that we shouldn't get both a parent AND file Inode - that's a logic error
            assert(uuid_is_null(fmsg->Message.Fuse.Request.Parameters.Stat.ParentInode));
            uuid_unparse(fmsg->Message.Fuse.Request.Parameters.Stat.Inode, uuid_buffer);
            finobj = finesse_object_lookup_by_uuid(&fmsg->Message.Fuse.Request.Parameters.Stat.Inode);
            if (NULL == finobj) {
                status = FinesseSendStatResponse(fsh, Client, Message, &zerostat, 0, EBADF);
                assert(0 == status);
                break;
            }
        }

        // We need to getattr at this point
        fuse_request    = FinesseAllocFuseRequest(se);
        finesse_request = (struct finesse_req *)fuse_request;
        fuse_request->ctr++;  // ensure's it doesn't go away before we're done with it
        fuse_request->opcode = FUSE_GETATTR;
        finesse_original_ops->getattr(fuse_request, ino, NULL);

        FinesseWaitForFuseRequestCompletion(finesse_request);

        assert(finesse_request->iov_count > 0);
        out = finesse_request->iov[0].iov_base;

        if ((0 != out->error) || (finesse_request->iov_count < 2) ||
            (finesse_request->iov[1].iov_len < sizeof(struct fuse_entry_out))) {
            fuse_log(FUSE_LOG_ERR, "Finesse: %s returning %d for %s\n", __func__, out->error, Name);
            status = FinesseSendStatResponse(fsh, Client, Message, NULL, 0, out->error);
            break;
        }

        // Now we have the attributes
        arg = finesse_request->iov[1].iov_base;

        assert(NULL != arg);
        // At the present time, this logic does NOT deal with symlink(s)
        // correctly. Fix as necessary...
        assert(0 == S_ISLNK(arg->attr.mode));

        memset(&statout, 0, sizeof(statout));
        statout.st_ino          = arg->attr.ino;
        statout.st_mode         = arg->attr.mode;
        statout.st_nlink        = arg->attr.nlink;
        statout.st_uid          = arg->attr.uid;
        statout.st_gid          = arg->attr.gid;
        statout.st_rdev         = arg->attr.rdev;
        statout.st_size         = arg->attr.size;
        statout.st_blksize      = arg->attr.blksize;
        statout.st_blocks       = arg->attr.blocks;
        statout.st_atim.tv_sec  = arg->attr.atime;
        statout.st_atim.tv_nsec = arg->attr.atimensec;
        statout.st_mtim.tv_sec  = arg->attr.mtime;
        statout.st_mtim.tv_nsec = arg->attr.mtimensec;
        statout.st_ctim.tv_nsec = arg->attr.ctime;
        statout.st_ctim.tv_nsec = arg->attr.ctimensec;
        timeout                 = (double)arg->attr_valid + (((double)arg->attr_valid) / 1.0e9);

        status = FinesseSendStatResponse(fsh, Client, Message, &statout, timeout, out->error);
        assert(0 == status);

        break;
    }

    if (NULL != parentobj) {
        finesse_object_release(parentobj);
        parentobj = NULL;
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

FinesseServerFunctionHandler FinesseServerFuseStat = Stat;
