/*
 * (C) Copyright 2018 Tony Mason
 * All Rights Reserved
*/

#include "finesse.pb-c.h"
#include "finesse.h"
#include "mqcomm.h"
#include <errno.h>
#include <stdlib.h>
#include <memory.h>

int FinesseSendDirMapRequest(finesse_client_handle_t FinesseClientHandle, uint64_t *RequestId, uuid_t *Key, char *Path)
{
    Finesse__FinesseRequest req = FINESSE__FINESSE_REQUEST__INIT;
    Finesse__FinesseMessageHeader header = FINESSE__FINESSE_MESSAGE_HEADER__INIT;
    Finesse__FinesseRequest__DirMap dirmap = FINESSE__FINESSE_REQUEST__DIR_MAP__INIT;
    void *buffer = NULL;
    size_t buffer_len = 0;
    size_t packed_buffer_len = 0;
    int status = -ENOSYS;

    while (NULL == buffer) {
        finesse_set_client_message_header(FinesseClientHandle, &header, FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__DIR_MAP);

        req.header = &header;
        req.clientuuid.data = (uint8_t *)finesse_get_client_uuid(FinesseClientHandle);
        req.clientuuid.len = sizeof(uuid_t);
        req.request_case = FINESSE__FINESSE_REQUEST__REQUEST_DIR_MAP_REQ;
        req.dirmapreq = &dirmap;
        if (((NULL == Key) && (NULL == Path)) ||
            ((NULL != Key) && (NULL != Path))) {
            // must specify one or the other but not both
            status = EINVAL;
            break;
        }
        if (NULL != Key) {
            dirmap.key_type_case = FINESSE__FINESSE_REQUEST__DIR_MAP__KEY_TYPE_KEY;
            dirmap.key.data = (uint8_t *)Key;
            dirmap.key.len = sizeof(uuid_t);
        }
        else {
            assert(NULL != Path);
            dirmap.key_type_case = FINESSE__FINESSE_REQUEST__DIR_MAP__KEY_TYPE_NAME;
            dirmap.name = Path;
        }

        // TODO: should this be based upon the max size of the client queue?
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

int FinesseSendDirMapResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, char *Path, int64_t Result)
{
    Finesse__FinesseResponse rsp = FINESSE__FINESSE_RESPONSE__INIT;
    Finesse__FinesseMessageHeader header = FINESSE__FINESSE_MESSAGE_HEADER__INIT;
    Finesse__FinesseResponse__DirMap dmrsp = FINESSE__FINESSE_RESPONSE__DIR_MAP__INIT;
    void *buffer = NULL;
    size_t buffer_len = 0;
    size_t packed_buffer_len = 0;
    int status = -ENOSYS;

    while (NULL == buffer) {
        finesse_set_server_message_header(FinesseServerHandle, &header, RequestId, FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__DIR_MAP);

        rsp.header = &header;
        rsp.status = Result;
        rsp.response_case = FINESSE__FINESSE_RESPONSE__RESPONSE_DIR_MAP_RSP; // no response structure
        rsp.dirmaprsp = &dmrsp;
        dmrsp.name = Path;

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

        status = FinesseSendResponse(FinesseServerHandle, ClientUuid, buffer);

        break;
    }


    // cleanup
    if (NULL != buffer) {
        free(buffer);
        buffer = NULL;
    }

    return status;
}


int FinesseGetDirMapResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId)
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

        status = 0;
        break;
    }

    if (NULL != buffer) {
        FinesseFreeClientResponse(FinesseClientHandle, buffer);
        buffer = NULL;
    }

    return status;
}