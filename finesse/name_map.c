/*
 * (C) Copyright 2017 Tony Mason
 * All Rights Reserved
*/

#include "finesse.pb-c.h"
#include "finesse.h"
#include "mqcomm.h"
#include <errno.h>
#include <stdlib.h>
#include <memory.h>

int FinesseSendNameMapRequest(finesse_client_handle_t FinesseClientHandle, char *NameToMap, uint64_t *RequestId)
{
    Finesse__FinesseRequest req = FINESSE__FINESSE_REQUEST__INIT;
    Finesse__FinesseMessageHeader header = FINESSE__FINESSE_MESSAGE_HEADER__INIT;
    Finesse__FinesseRequest__NameMap name_map = FINESSE__FINESSE_REQUEST__NAME_MAP__INIT;

    void *buffer = NULL;
    size_t buffer_len = 0;
    size_t packed_buffer_len = 0;
    int status = -ENOSYS;

    while (NULL == buffer) {
        finesse_set_client_message_header(FinesseClientHandle, &header, FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__NAME_MAP);

        req.header = &header;
        req.clientuuid.data = (uint8_t *)finesse_get_client_uuid(FinesseClientHandle);
        req.clientuuid.len = sizeof(uuid_t);
        req.request_case = FINESSE__FINESSE_REQUEST__REQUEST_NAME_MAP_REQ;
        req.namemapreq = &name_map;
        name_map.name = NameToMap;

        buffer_len = finesse__finesse_request__get_packed_size(&req);

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

int FinesseSendNameMapResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, uuid_t *MapKey, int64_t Result)
{
    Finesse__FinesseResponse rsp = FINESSE__FINESSE_RESPONSE__INIT;
    Finesse__FinesseMessageHeader header = FINESSE__FINESSE_MESSAGE_HEADER__INIT;
    Finesse__FinesseResponse__NameMap name_map = FINESSE__FINESSE_RESPONSE__NAME_MAP__INIT;
    void *buffer = NULL;
    size_t buffer_len = 0;
    size_t packed_buffer_len = 0;
    int status = -ENOSYS;

    while (NULL == buffer) {
        finesse_set_server_message_header(FinesseServerHandle, &header, RequestId, FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__NAME_MAP);

        rsp.header = &header;
        rsp.status = Result;
        rsp.response_case = FINESSE__FINESSE_RESPONSE__RESPONSE_NAME_MAP_RSP;
        rsp.namemaprsp = &name_map;
        name_map.key.data = (uint8_t *) MapKey;
        name_map.key.len = sizeof(uuid_t);

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


int FinesseGetNameMapResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId, uuid_t *MapKey)
{
    Finesse__FinesseResponse *rsp = NULL;
    void *buffer = NULL;
    size_t buffer_len = 0;
    int status;

    while (NULL == buffer) {
        status = FinesseGetClientResponse(FinesseClientHandle, &buffer, &buffer_len);

        if (0 != status) {
            break;
        }

        rsp = finesse__finesse_response__unpack(NULL, buffer_len, (const uint8_t *)buffer);

        if (NULL == rsp) {
            status = -EINVAL;
            break;
        }

        assert(rsp->header->messageid == RequestId);
        assert(sizeof(uuid_t) == rsp->namemaprsp->key.len);
        memcpy(MapKey, rsp->namemaprsp->key.data, sizeof(uuid_t));

        status = rsp->status;
        break;
    }

    if (NULL != buffer) {
        FinesseFreeClientResponse(FinesseClientHandle, buffer);
        buffer = NULL;
    }

    return status;
}

int FinesseSendNameMapReleaseRequest(finesse_client_handle_t FinesseClientHandle, uuid_t *MapKey, uint64_t *RequestId)
{
    Finesse__FinesseRequest req = FINESSE__FINESSE_REQUEST__INIT;
    Finesse__FinesseMessageHeader header = FINESSE__FINESSE_MESSAGE_HEADER__INIT;
    Finesse__FinesseRequest__NameMapRelease nmr = FINESSE__FINESSE_REQUEST__NAME_MAP_RELEASE__INIT;
    void *buffer = NULL;
    size_t buffer_len = 0;
    size_t packed_buffer_len = 0;
    int status = -ENOSYS;

    while (NULL == buffer) {
        finesse_set_client_message_header(FinesseClientHandle, &header, FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__NAME_MAP_RELEASE);

        req.header = &header;
        req.clientuuid.data = (uint8_t *)finesse_get_client_uuid(FinesseClientHandle);
        req.clientuuid.len = sizeof(uuid_t);
        req.request_case = FINESSE__FINESSE_REQUEST__REQUEST_NAME_MAP_RELEASE_REQ;
        req.namemapreleasereq = &nmr;
        nmr.key.data = (uint8_t *)MapKey;
        nmr.key.len = sizeof(uuid_t);

        buffer_len = finesse__finesse_request__get_packed_size(&req);

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

int FinesseSendNameMapReleaseResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, int64_t Result)
{
    Finesse__FinesseResponse rsp = FINESSE__FINESSE_RESPONSE__INIT;
    Finesse__FinesseMessageHeader header = FINESSE__FINESSE_MESSAGE_HEADER__INIT;
    void *buffer = NULL;
    size_t buffer_len = 0;
    size_t packed_buffer_len = 0;
    int status = -ENOSYS;

    while (NULL == buffer) {
        finesse_set_server_message_header(FinesseServerHandle, &header, RequestId, FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__NAME_MAP_RELEASE);

        rsp.header = &header;
        rsp.status = Result;
        rsp.response_case = FINESSE__FINESSE_RESPONSE__RESPONSE__NOT_SET; // this is the default anyway

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

int FinesseGetNameMapReleaseResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId)
{
    Finesse__FinesseResponse *rsp = NULL;
    void *buffer = NULL;
    size_t buffer_len = 0;
    int status;

    while (NULL == buffer) {
        status = FinesseGetClientResponse(FinesseClientHandle, &buffer, &buffer_len);

        if (0 != status) {
            break;
        }

        rsp = finesse__finesse_response__unpack(NULL, buffer_len, (const uint8_t *)buffer);

        if (NULL == rsp) {
            status = -EINVAL;
            break;
        }

        assert(rsp->header->messageid == RequestId);

        status = (int) rsp->status;
        break;
    }

    if (NULL != buffer) {
        FinesseFreeClientResponse(FinesseClientHandle, buffer);
        buffer = NULL;
    }

    return status;
}
