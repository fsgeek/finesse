/*
  Copyright (C) 2020  Tony Mason <fsgeek@cs.ubc.ca>
*/
#include "fs-internal.h"


int FinesseServerNativeMapRequest(struct fuse_session *se, void *Client, fincomm_message Message)
{
    finesse_server_handle_t fsh = NULL;
    static uuid_t null_uuid;
    struct fuse_req *fuse_request;
    struct finesse_req *finesse_request;
    finesse_msg *fmsg = (finesse_msg *)Message->Data;
    int status = 0;
    size_t mp_length = strlen(se->mountpoint);
    struct fuse_out_header *out = NULL;
    struct fuse_entry_out *arg = NULL;
    finesse_object_t *finobj = NULL;
    uuid_t uuid;

    fsh = (finesse_server_handle_t) se->server_handle;

    if (NULL == fsh) {
        return ENOTCONN;
    }

    // Presently, we don't handle openat
    if (!uuid_is_null(fmsg->Message.Native.Request.Parameters.Map.Parent)) {
        return FinesseSendNameMapResponse(fsh, Client, Message, &null_uuid, ENOTSUP);
    }


    if ((strlen(fmsg->Message.Native.Request.Parameters.Map.Name) < mp_length) ||
        (strncmp(fmsg->Message.Native.Request.Parameters.Map.Name, se->mountpoint, mp_length))) {
        fuse_log(FUSE_LOG_ERR, "Finesse: %s returning %d for %s\n", __PRETTY_FUNCTION__, ENOTDIR, fmsg->Message.Native.Request.Parameters.Map.Name);
        return FinesseSendNameMapResponse(fsh, Client, Message, &null_uuid, ENOTDIR);      
    }

    // We need to do a lookup here
    fuse_request = FinesseAllocFuseRequest(se);
    finesse_request = (struct finesse_req *)fuse_request;
    assert(NULL != fuse_request);

    if (NULL == fuse_request) {
        fprintf(stderr, "%s @ %d (%s): alloc failure\n", __FILE__, __LINE__, __FUNCTION__);
        // TODO: fix this function's prototype
        return FinesseSendNameMapResponse(fsh, Client, Message, &null_uuid, ENOMEM);      
    }

    finesse_request->completed = 0;

    fuse_request->finesse.message = Message;
    fuse_request->finesse.client = Client;
    fuse_request->opcode = FUSE_LOOKUP; // Fuse internal call
    fuse_request->se = se;
    finesse_original_ops->lookup(fuse_request, FUSE_ROOT_ID, &fmsg->Message.Native.Request.Parameters.Map.Name[mp_length]);

    FinesseWaitForFuseRequestCompletion(finesse_request);

    // At this point the lookup is done - if we got an inode number, we will
    // find a uuid for it (existing or new).

    assert(finesse_request->iov_count > 0);
    out = finesse_request->iov[0].iov_base;

    if (finesse_request->iov_count < 2) {
        // I'm assuming this is an error state?
        assert(0 != out);
        status = FinesseSendNameMapResponse(fsh, fuse_request->finesse.client, fuse_request->finesse.message, &uuid, status);
        assert(0 == status);
        FinesseFreeFuseRequest(fuse_request);
        return 0;
    }

    arg = finesse_request->iov[1].iov_base;
    assert(finesse_request->iov[1].iov_len >= sizeof(struct fuse_entry_out));

    uuid_generate_time_safe(uuid);
    finobj = finesse_object_create(arg->attr.ino, &uuid);
    assert(NULL != finobj);

    // Note: if the inode is already known, we get back that current object
    // with a different uuid than what we specified! 
    status = FinesseSendNameMapResponse(fsh, fuse_request->finesse.client, fuse_request->finesse.message, &finobj->uuid, status);
    assert(0 == status); // may have to handle the client disconnection case here

    // Just clean up
    finesse_object_release(finobj);
    finobj = NULL;
    fuse_request->finesse.client = NULL; // message is gone
    FinesseFreeFuseRequest(fuse_request);
    fuse_request = NULL;
    finesse_request = NULL;

    return 0;
 
}

int FinesseServerNativeMapReleaseRequest(finesse_server_handle_t Fsh, void *Client, fincomm_message Message)
{
    finesse_object_t *object = NULL;
    finesse_msg *fmsg = (finesse_msg *)Message->Data;
    int status = 0;

    object = finesse_object_lookup_by_uuid(&fmsg->Message.Native.Request.Parameters.MapRelease.Key);
    if (NULL != object) {
        finesse_object_release(object);
    }

    if (NULL != Fsh) {
        // TODO: fix this function prototype/call
        status = FinesseSendNameMapReleaseResponse(Fsh, Client, Message, 0);
    }

    return status;
}