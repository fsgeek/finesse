/*
 * (C) Copyright 2017 Tony Mason
 * All Rights Reserved
 */


#include "finesse-internal.h"
#include <stdarg.h>
#include <uuid/uuid.h>
#include "finesse.pb-c.h"
#include "mqcomm.h"

static int fin_unlink_call(const char *unlinkfile_name);

static int fin_unlink(const char *pathname)
{
    typedef int (*orig_unlink_t)(const char *pathname); 
    static orig_unlink_t orig_unlink = NULL;

    if (NULL == orig_unlink) {
        orig_unlink = (orig_unlink_t)dlsym(RTLD_NEXT, "unlink");

        assert(NULL != orig_unlink);
        if (NULL == orig_unlink) {
            return EACCES;
        }
    }

    return orig_unlink(pathname);
}

static int fin_unlinkat(int dirfd, const char *pathname, int flags)
{
    typedef int (*orig_unlinkat_t)(int dirfd, const char *pathname, int flags);
    static orig_unlinkat_t orig_unlinkat = NULL;

    if (NULL == orig_unlinkat) {
        orig_unlinkat = (orig_unlinkat_t) dlsym(RTLD_NEXT, "unlinkat");

        assert(NULL != orig_unlinkat);
        if (NULL == orig_unlinkat) {
            return EACCES;
        }
    }

    return orig_unlinkat(dirfd, pathname, flags);
}

int finesse_unlink(const char *pathname)
{
    int status;

    finesse_init();

    if (0 == finesse_check_prefix(pathname)) {
        // not of interest
        return fin_unlink(pathname);
    }

    status = fin_unlink_call(pathname);

    if (0 > status) {
        status = fin_unlink(pathname);
    }

    return status;
}

int finesse_unlinkat(int dirfd, const char *pathname, int flags)
{
    int status;

    // TODO: implement the lookup here and really implement this
    status = fin_unlinkat(dirfd, pathname, flags);

    return status;
}



static int fin_unlink_call(const char *unlinkfile_name)
{
    int status;
    uint64_t req_id;

    status = FinesseSendUnlinkRequest(finesse_client_handle, unlinkfile_name, &req_id);
    while (0 == status) {
        status = FinesseGetUnlinkResponse(finesse_client_handle, req_id);
        break;
    }

    return status;
}

int FinesseSendUnlinkRequest(finesse_client_handle_t FinesseClientHandle, const char *NameToUnlink, uint64_t *RequestId)
{
    Finesse__FinesseRequest req = FINESSE__FINESSE_REQUEST__INIT;
    Finesse__FinesseMessageHeader header = FINESSE__FINESSE_MESSAGE_HEADER__INIT;
    Finesse__FinesseRequest__Unlink unlink_path = FINESSE__FINESSE_REQUEST__UNLINK__INIT;
    void *buffer = NULL;
    size_t buffer_len = 0;
    size_t packed_buffer_len = 0;
    int status = -ENOSYS;

    while (NULL == buffer) {
        finesse_set_client_message_header(FinesseClientHandle, &header, FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__PATH_SEARCH);

        req.header = &header;
        req.clientuuid.data = (uint8_t *)finesse_get_client_uuid(FinesseClientHandle);
        req.clientuuid.len = sizeof(uuid_t);
        req.request_case = FINESSE__FINESSE_REQUEST__REQUEST_UNLINK_REQ;
        req.unlinkreq = &unlink_path;

        unlink_path.name = (char *)(uintptr_t)NameToUnlink;

        buffer_len = finesse__finesse_request__get_packed_size(&req);

        // TODO: note that if it is too big we'll have to figure out a different way
        // to send this information across (and I *do* expect that to happen)
        // my expectation is that I'll need to create a shared memory buffer for this.
        if (buffer_len > FINESSE_MQ_MAX_MESSAGESIZE) {
            status = -EINVAL;
            break;
        }

        buffer = malloc(buffer_len);
        if (NULL == buffer) {
            status = -ENOMEM;
            break;
        }

        packed_buffer_len = finesse__finesse_request__pack(&req, (uint8_t *)buffer);
        assert(buffer_len == packed_buffer_len);

        status = FinesseSendRequest(FinesseClientHandle, buffer, buffer_len);

        break;
    }


    // cleanup
    if (NULL != buffer) {
        free(buffer);
        buffer = NULL;
    }

    *RequestId = header.messageid;
    return status;
}

int FinesseSendUnlinkResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, int64_t Result)
{
    Finesse__FinesseResponse rsp = FINESSE__FINESSE_RESPONSE__INIT;
    Finesse__FinesseMessageHeader header = FINESSE__FINESSE_MESSAGE_HEADER__INIT;
    void *buffer = NULL;
    size_t buffer_len = 0;
    size_t packed_buffer_len = 0;
    int status = -ENOSYS;

    while (NULL == buffer) {
        finesse_set_server_message_header(FinesseServerHandle, &header, RequestId, FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__UNLINK);

        rsp.header = &header;
        rsp.status = Result;
        rsp.response_case = FINESSE__FINESSE_RESPONSE__RESPONSE__NOT_SET;

        buffer_len = finesse__finesse_response__get_packed_size(&rsp);

        if (buffer_len > finesse_get_max_message_size(FinesseServerHandle)) {
            status = -EINVAL;
            break;
        }

        buffer = malloc(buffer_len);
        if (NULL == buffer) {
            status = -ENOMEM;
            break;
        }

        packed_buffer_len = finesse__finesse_response__pack(&rsp, (uint8_t *)buffer);
        assert(buffer_len == packed_buffer_len);

        status = FinesseSendResponse(FinesseServerHandle, ClientUuid, buffer, buffer_len);

        break;
    }


    // cleanup
    if (NULL != buffer) {
        free(buffer);
        buffer = NULL;
    }

    return status;
}

int FinesseGetUnlinkResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId)
{
    Finesse__FinesseResponse *rsp = NULL;
    void *buffer = NULL;
    size_t buffer_len = 0;
    int status;

    while (NULL == buffer)
    {
        status = FinesseGetClientResponse(FinesseClientHandle, &buffer, &buffer_len);

        if (0 != status)
        {
            break;
        }

        rsp = finesse__finesse_response__unpack(NULL, buffer_len, (const uint8_t *)buffer);

        if (NULL == rsp)
        {
            status = -EINVAL;
            break;
        }

        assert(rsp->header->messageid == RequestId);

        status = (int)rsp->status;
        break;
    }

    if (NULL != buffer)
    {
        FinesseFreeClientResponse(FinesseClientHandle, buffer);
        buffer = NULL;
    }

    return status;
}
