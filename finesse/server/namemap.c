/*
  Copyright (C) 2020  Tony Mason <fsgeek@cs.ubc.ca>
*/
#include "fs-internal.h"

//
// We have to do this name lookup trick in multiple places.  This part of the logic
// just maps the name to the corresponding fuse_ino_t.  The caller may just use it
// or could keep it.  Note that the caller must release it at some point!
//
int FinesseServerInternalNameLookup(struct fuse_session *se, fuse_ino_t Parent, const char *Name, struct statx *attr)
{
    struct fuse_req *        fuse_request    = NULL;
    struct finesse_req *     finesse_request = NULL;
    int                      status          = 0;
    size_t                   mp_length       = strlen(se->mountpoint);
    struct fuse_out_header * out             = NULL;
    struct fuse_entry_param *arg             = NULL;

    assert(NULL != attr);
    memset(attr, 0, sizeof(struct statx));

    // We need to do a lookup here - allocate a request structure
    fuse_request    = FinesseAllocFuseRequest(se);
    finesse_request = (struct finesse_req *)fuse_request;

    if (NULL == fuse_request) {
        fuse_log(FUSE_LOG_ERR, "Finesse: %s returning %d for %s\n", __func__, ENOMEM, Name);
        return ENOMEM;
    }

    while (NULL != fuse_request) {
        finesse_request->completed = 0;
        fuse_request->ctr++;                 // we want to hold on to this until we are done with it
        fuse_request->opcode = FUSE_LOOKUP;  // Fuse internal call
        finesse_original_ops->lookup(fuse_request, Parent, &Name[mp_length]);

        FinesseWaitForFuseRequestCompletion(finesse_request);

        // At this point the lookup is done - if we got an inode number, we will
        // find a uuid for it (existing or new).
        assert(finesse_request->iov_count > 0);
        out = finesse_request->iov[0].iov_base;
        if ((0 != out->error) || (finesse_request->iov_count < 2) ||
            (finesse_request->iov[1].iov_len < sizeof(struct fuse_entry_out))) {
            fuse_log(FUSE_LOG_ERR, "Finesse: %s returning %d for %s\n", __func__, out->error, Name);
            status = out->error;
            break;
        }

        arg                       = finesse_request->iov[1].iov_base;
        attr->stx_mask            = 0;
        attr->stx_blksize         = arg->attr.st_blksize;
        attr->stx_attributes      = 0;
        attr->stx_nlink           = arg->attr.st_nlink;
        attr->stx_uid             = arg->attr.st_uid;
        attr->stx_gid             = arg->attr.st_gid;
        attr->stx_mode            = arg->attr.st_mode;
        attr->stx_ino             = arg->attr.st_ino;
        attr->stx_size            = arg->attr.st_size;
        attr->stx_blocks          = arg->attr.st_blocks;
        attr->stx_attributes_mask = STATX_BASIC_STATS;  // everything except "btime"
        attr->stx_atime.tv_sec    = arg->attr.st_atim.tv_sec;
        attr->stx_atime.tv_nsec   = arg->attr.st_atim.tv_nsec;
        // TODO: would be great to get the creation time back
        attr->stx_ctime.tv_sec  = arg->attr.st_ctim.tv_sec;
        attr->stx_ctime.tv_nsec = arg->attr.st_ctim.tv_nsec;
        attr->stx_mtime.tv_sec  = arg->attr.st_mtim.tv_sec;
        attr->stx_mtime.tv_nsec = arg->attr.st_mtim.tv_nsec;
        status                  = 0;
        break;
    }

    if (NULL != fuse_request) {
        FinesseFreeFuseRequest(fuse_request);
        fuse_request    = NULL;
        finesse_request = NULL;
    }

    return status;
}

//
// We have to do this name lookup trick in multiple places, so I'm extracting the code
// here.  This asks the FUSE file system for the inode number.
//
int FinesseServerInternalNameMapRequest(struct fuse_session *se, uuid_t *Parent, const char *Name, finesse_object_t **Finobj)
{
    struct fuse_req *fuse_request;
    int              status         = 0;
    size_t           mp_length      = strlen(se->mountpoint);
    int              created_finobj = 0;
    uuid_t           uuid;
    ino_t            parent_ino = FUSE_ROOT_ID;
    fuse_ino_t       ino        = 0;
    struct statx     statxbuf;

    if ((NULL != Parent) && !uuid_is_null(*Parent)) {
        finesse_object_t *parent_fin_obj = NULL;

        // look it up
        parent_fin_obj = finesse_object_lookup_by_uuid(Parent);
        if (NULL == parent_fin_obj) {
            fuse_log(FUSE_LOG_ERR, "Finesse: %s returning EBADF due to bad parent\n", __func__);
            return EBADF;
        }

        parent_ino = parent_fin_obj->inode;
        finesse_object_release(parent_fin_obj);
        parent_fin_obj = NULL;
    }

    assert(NULL != Finobj);

    if ((strlen(Name) < mp_length) || (strncmp(Name, se->mountpoint, mp_length))) {
        fuse_log(FUSE_LOG_ERR, "Finesse: %s returning %d for %s\n", __func__, ENOTDIR, Name);
        return ENOTDIR;
    }

    // We need to do a lookup here - allocate a request structure
    fuse_request = FinesseAllocFuseRequest(se);

    if (NULL == fuse_request) {
        fuse_log(FUSE_LOG_ERR, "Finesse: %s returning %d for %s\n", __func__, ENOMEM, Name);
        return ENOMEM;
    }

    while (NULL != fuse_request) {
        *Finobj = NULL;
        status  = FinesseServerInternalNameLookup(se, parent_ino, Name, &statxbuf);

        if (0 != status) {
            break;
        }
        ino = statxbuf.stx_ino;
        assert(0 != ino);  // invalid inode

        uuid_generate_time_safe(uuid);
        *Finobj = finesse_object_create(ino, &uuid);
        assert(NULL != *Finobj);
        if (0 == uuid_compare(uuid, (*Finobj)->uuid)) {
            created_finobj = 1;
        }
        else {
            created_finobj = 0;
        }

        // No need to try again.
        status = 0;
        break;
    }

    // Just clean up
    if (created_finobj) {
        // Creation returns with TWO references - I want to release mine, let the caller
        // release theres if they don't need it any longer.
        finesse_object_release(*Finobj);
    }

    if (NULL != fuse_request) {
        FinesseFreeFuseRequest(fuse_request);
        fuse_request = NULL;
    }

    return status;
}

int FinesseServerNativeMapRequest(struct fuse_session *se, void *Client, fincomm_message Message)
{
    finesse_server_handle_t fsh    = NULL;
    finesse_msg *           fmsg   = (finesse_msg *)Message->Data;
    int                     status = 0;
    finesse_object_t *      finobj = NULL;

    fsh = (finesse_server_handle_t)se->server_handle;

    if (NULL == fsh) {
        return ENOTCONN;
    }

    // We need to do a lookup here
    status = FinesseServerInternalNameMapRequest(se, &fmsg->Message.Native.Request.Parameters.Map.Parent,
                                                 fmsg->Message.Native.Request.Parameters.Map.Name, &finobj);

    if (0 == status) {
        assert(NULL != finobj);  // that wouldn't make sense
        status = FinesseSendNameMapResponse(fsh, Client, Message, &finobj->uuid, 0);
        finobj = NULL;
    }
    else {
        assert(NULL == finobj);
        status = FinesseSendNameMapResponse(fsh, Client, Message, NULL, ENOENT);
    }

    if (0 == status) {
        FinesseCountNativeResponse(FINESSE_NATIVE_RSP_MAP);
    }

    // The operation worked, even if the result was ENOENT
    return 0;
}

int FinesseServerNativeMapReleaseRequest(finesse_server_handle_t Fsh, void *Client, fincomm_message Message)
{
    finesse_object_t *object = NULL;
    finesse_msg *     fmsg   = (finesse_msg *)Message->Data;
    int               status = 0;

    object = finesse_object_lookup_by_uuid(&fmsg->Message.Native.Request.Parameters.MapRelease.Key);
    if (NULL != object) {
        finesse_object_release(object);
    }

    if (NULL != Fsh) {
        status = FinesseSendNameMapReleaseResponse(Fsh, Client, Message, 0);

        if (0 == status) {
            FinesseCountNativeResponse(FINESSE_NATIVE_RSP_MAP_RELEASE);
        }
    }

    return status;
}
