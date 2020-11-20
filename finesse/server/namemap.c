/*
  Copyright (C) 2020  Tony Mason <fsgeek@cs.ubc.ca>
*/
#include "fs-internal.h"

//
// We have to do this name lookup trick in multiple places.  This part of the logic
// just maps the name to the corresponding fuse_ino_t.  The caller may just use it
// or could keep it.  Note that the caller must release it at some point!
//
// Note: we should not call this with a path name.
//
int FinesseServerInternalNameLookup(struct fuse_session *se, fuse_ino_t Parent, const char *Name, struct statx *attr)
{
    struct fuse_req *       fuse_request    = NULL;
    struct finesse_req *    finesse_request = NULL;
    int                     status          = 0;
    struct fuse_out_header *out             = NULL;
    struct fuse_entry_out * arg             = NULL;

    assert(NULL != attr);
    memset(attr, 0, sizeof(struct statx));
    assert(NULL != Name);
    assert(NULL == index(Name, '/'));  // we no longer support path names - use the PathMap version!

    // We need to do a lookup here - allocate a request structure
    fuse_request    = FinesseAllocFuseRequest(se);
    finesse_request = (struct finesse_req *)fuse_request;

    if (NULL == fuse_request) {
        fuse_log(FUSE_LOG_ERR, "Finesse: %s:%d returning %d for %s\n", __func__, __LINE__, ENOMEM, Name);
        return ENOMEM;
    }

    while (NULL != fuse_request) {
        finesse_request->completed = 0;
        fuse_request->ctr++;                 // we want to hold on to this until we are done with it
        fuse_request->opcode = FUSE_LOOKUP;  // Fuse internal call
        finesse_original_ops->lookup(fuse_request, Parent, Name);

        FinesseWaitForFuseRequestCompletion(finesse_request);

        // At this point the lookup is done - if we got an inode number, we will
        // find a uuid for it (existing or new).
        assert(finesse_request->iov_count > 0);
        out = finesse_request->iov[0].iov_base;
        if ((0 != out->error) || (finesse_request->iov_count < 2) ||
            (finesse_request->iov[1].iov_len < sizeof(struct fuse_entry_out))) {
            fuse_log(FUSE_LOG_INFO, "Finesse: %s:%d returning %d for %s\n", __func__, __LINE__, out->error, Name);
            status = out->error;
            break;
        }

        arg                  = finesse_request->iov[1].iov_base;
        attr->stx_mask       = 0;
        attr->stx_blksize    = arg->attr.blksize;
        attr->stx_attributes = 0;
        attr->stx_nlink      = arg->attr.nlink;
        attr->stx_uid        = arg->attr.uid;
        attr->stx_gid        = arg->attr.gid;
        attr->stx_mode       = arg->attr.mode;
        attr->stx_ino        = arg->nodeid;
        assert(arg->nodeid == arg->attr.ino);
        attr->stx_size            = arg->attr.size;
        attr->stx_blocks          = arg->attr.blocks;
        attr->stx_attributes_mask = STATX_BASIC_STATS;  // everything except "btime"
        attr->stx_atime.tv_sec    = arg->attr.atime;
        attr->stx_atime.tv_nsec   = arg->attr.atimensec;
        // TODO: would be great to get the creation time back
        attr->stx_ctime.tv_sec  = arg->attr.ctime;
        attr->stx_ctime.tv_nsec = arg->attr.ctimensec;
        attr->stx_mtime.tv_sec  = arg->attr.mtime;
        attr->stx_mtime.tv_nsec = arg->attr.mtimensec;
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
// This is a path map walk request
static int FinsesseServerInternalPathMapRequest(struct fuse_session *se, ino_t ParentInode, uuid_t *ParentUuid, const char *Name,
                                                int Flags, finesse_object_t **Finobj)
{
    finesse_object_t *                       parent_fin_obj = NULL;
    int                                      status         = EBADF;
    FinesseServerPathResolutionParameters_t *parameters     = NULL;
    ino_t                                    ino            = 0;
    size_t                                   mp_length      = strlen(se->mountpoint);
    uuid_t                                   uuid;
    int                                      created_finobj = 0;

    // don't call this with the mount point!
    assert(mp_length < strlen(Name) || 0 != memcmp(Name, se->mountpoint, mp_length));

    // TODO: support flags
    // which flags matter?
    //
    // Docs divide flags into "file creation flags" and "file status flags".  We don't
    // seem to care about status flags, though ultimately, it is the file system that
    // has to enforce these things.  For now, at least, we're implementing an augmentation
    // bypass - the file handle still goes through the main kernel path, so things like
    // verifying access are done server side anyway.  If we moved to a full bypass
    // model, we'd need to deconstruct and enforce these on the server.  For example, O_RDWR
    // wouldn't be granted for a file where the file doesn't grant read
    //
    // The creation flags:
    //
    // O_CLOEXEC, O_CREAT, O_DIRECTORY, O_EXCL, O_NOCTTY, O_NOFOLLOW, O_TMPFILE, and O_TRUNC
    //
    // O_CREAT and O_TMPFILE are related; the latter seems to impart a "hint" about the
    // purpose of the create operation.  In this case, the file MAY be created (O_EXCL) if
    // it does not exist.
    //
    // O_DIRECTORY means that what we are trying to open must be a directory; so we can handle
    // that case on the end, when we look at its attributes.
    //
    // O_EXCL is about the file handle; we don't care about that one.
    //
    // O_NOCTTY really does seem to be something we care about on the client, not the server.
    //
    // O_NOFOLLOw IS something we want to handle, since it affects the way we process symlinks; do we want to do that
    // server side, or client side?
    //
    // O_TRUNC relates to existing files; we could handle that server side.
    //
    // Some of the challenge here will be in communicating the nuances of the server side actions.
    //

    // assert(0 == Flags);  // none supported at the moment

    if (0 == ParentInode) {
        ParentInode = FUSE_ROOT_ID;
    }

    // We can't specify both of these!
    assert((FUSE_ROOT_ID == ParentInode) || ((NULL == ParentUuid) || uuid_is_null(*ParentUuid)));

    while (1) {
        if ((NULL != ParentUuid) && !uuid_is_null(*ParentUuid)) {
            // Need to get the inode number to override the default value

            parent_fin_obj = finesse_object_lookup_by_uuid(ParentUuid);
            if (NULL == parent_fin_obj) {
                status = EBADF;
                break;
            }
            ParentInode = parent_fin_obj->inode;

            // Done with this
            finesse_object_release(parent_fin_obj);
            parent_fin_obj = NULL;
        }

        assert(0 != ParentInode);  // sanity - shouldn't be able to get here.

        parameters = FinesseAllocateServerPathResolutionParameters(ParentInode, Name, Flags);
        assert(NULL != parameters);

        status = FinesseServerResolvePathName(se, parameters);

        if (0 != status) {
            break;
        }

        status = FinesseGetResolvedInode(parameters, &ino);

        if (0 != status) {
            break;
        }

        assert(0 != ino);

        uuid_generate_time_safe(uuid);
        assert(0 != ino);
        assert(!uuid_is_null(uuid));
        *Finobj = finesse_object_create(ino, &uuid);
        assert(NULL != *Finobj);
        assert(!uuid_is_null((*Finobj)->uuid));
        assert(0 != (*Finobj)->inode);
        if (0 == uuid_compare((*Finobj)->uuid, uuid)) {
            created_finobj = 1;
        }

        status = 0;
        break;
    }

    // Just clean up
    if (created_finobj) {
        // Creation returns with TWO references - I want to release mine, let the caller
        // release theres if they don't need it any longer.
        finesse_object_release(*Finobj);
    }

    // cleanup: parent_fin_obj
    if (NULL != parent_fin_obj) {
        finesse_object_release(parent_fin_obj);
        parent_fin_obj = NULL;
    }

    if (NULL != parameters) {
        FinesseFreeServerPathResolutionParameters(parameters);
        parameters = NULL;
    }

    return status;
}

// static int FinsesseServerInternalPathMapRequest(struct fuse_session *se, ino_t ParentInode, uuid_t *ParentUuid,
//                                                       const char *Name, int Flags, finesse_object_t **Finobj)

//
// We have to do this name lookup trick in multiple places, so I'm extracting the code
// here.  This asks the FUSE file system for the inode number.
//
static int FinesseServerInternalNameMapRequest(struct fuse_session *se, ino_t ParentInode, uuid_t *ParentUuid, const char *Name,
                                               int Flags, finesse_object_t **Finobj)
{
    struct fuse_req *fuse_request;
    int              status    = 0;
    size_t           mp_length = strlen(se->mountpoint);
    uuid_t           uuid;
    ino_t            parent_ino = FUSE_ROOT_ID;
    fuse_ino_t       ino        = 0;
    struct statx     statxbuf;
    int              created_finobj = 0;

    // assert(0 == Flags);  // don't handle these yet
    (void)Flags;  // TODO: decide if we need to process the flags.

    if ((0 != ParentInode) && !uuid_is_null(*ParentUuid)) {
        finesse_object_t *parent_fin_obj = NULL;

        // look it up
        parent_fin_obj = finesse_object_lookup_by_uuid(ParentUuid);
        if (NULL == parent_fin_obj) {
            fuse_log(FUSE_LOG_ERR, "Finesse: %s:%d returning EBADF due to bad parent\n", __func__, __LINE__);
            return EBADF;
        }

        parent_ino = parent_fin_obj->inode;
        finesse_object_release(parent_fin_obj);
        parent_fin_obj = NULL;
    }

    assert(NULL != Finobj);

    if ((strlen(Name) < mp_length) || (strncmp(Name, se->mountpoint, mp_length))) {
        fuse_log(FUSE_LOG_ERR, "Finesse: %s:%d returning %d for %s\n", __func__, __LINE__, ENOTDIR, Name);
        return ENOTDIR;
    }

    // We need to do a lookup here - allocate a request structure
    fuse_request = FinesseAllocFuseRequest(se);

    if (NULL == fuse_request) {
        fuse_log(FUSE_LOG_ERR, "Finesse: %s:%d returning %d for %s\n", __func__, __LINE__, ENOMEM, Name);
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
            assert((*Finobj)->inode == ino);
        }
        else {
            struct finesse_req *finesse_request = (struct finesse_req *)fuse_request;

            created_finobj = 0;

            // release the FUSE lookup
            finesse_request->completed = 0;
            fuse_request->ctr++;                 // we want to hold on to this until we are done with it
            fuse_request->opcode = FUSE_FORGET;  // Fuse internal call

            finesse_original_ops->forget(fuse_request, ino, 1);

            FinesseWaitForFuseRequestCompletion(finesse_request);

            // Note that the request will be freed below.
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

int FinesseServerInternalMapRequest(struct fuse_session *se, ino_t ParentInode, uuid_t *ParentUuid, const char *Name, int Flags,
                                    finesse_object_t **Finobj)
{
    size_t      mp_length     = strlen(se->mountpoint);
    const char *name_to_parse = Name;

    if ((strlen(name_to_parse) > mp_length) && (0 == memcmp(name_to_parse, se->mountpoint, mp_length))) {
        // The name includes the mount point, so we need to strip that off

        if (0 == ParentInode) {
            ParentInode = FUSE_ROOT_ID;
        }

        assert((FUSE_ROOT_ID == ParentInode) ||
               (1 == ParentInode));  // this really does require an absolute path, otherwise it makes no sense
        assert((NULL == ParentUuid) || uuid_is_null(*ParentUuid));  // again, can't be relative
        name_to_parse = Name + mp_length;
    }

    if (NULL != index(name_to_parse, '/')) {
        // contains a path name
        return FinsesseServerInternalPathMapRequest(se, ParentInode, ParentUuid, name_to_parse, Flags, Finobj);
    }

    return FinesseServerInternalNameMapRequest(se, ParentInode, ParentUuid, name_to_parse, Flags, Finobj);
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

    if (NULL == index(fmsg->Message.Native.Request.Parameters.Map.Name, '/')) {
        // This is a path lookup
        assert(0);  // not implemented
    }

    // We need to do a lookup here
    status = FinesseServerInternalMapRequest(se, 0, &fmsg->Message.Native.Request.Parameters.Map.Parent,
                                             fmsg->Message.Native.Request.Parameters.Map.Name,
                                             fmsg->Message.Native.Request.Parameters.Map.Flags, &finobj);

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
        // TODO: need to add a FUSE release for the original lookup
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
