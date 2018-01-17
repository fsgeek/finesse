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

int FinesseSendPathSearchRequest(finesse_client_handle_t FinesseClientHandle, char **Files, char **Paths, uint64_t *RequestId)
{
    Finesse__FinesseRequest req = FINESSE__FINESSE_REQUEST__INIT;
    Finesse__FinesseMessageHeader header = FINESSE__FINESSE_MESSAGE_HEADER__INIT;
    Finesse__FinesseRequest__PathSearch path_search = FINESSE__FINESSE_REQUEST__PATH_SEARCH__INIT;
    unsigned index;
    void *buffer = NULL;
    size_t buffer_len = 0;
    size_t packed_buffer_len = 0;
    int status = -ENOSYS;

    while (NULL == buffer) {
        finesse_set_client_message_header(FinesseClientHandle, &header, FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__PATH_SEARCH);

        req.header = &header;
        req.clientuuid.data = (uint8_t *)finesse_get_client_uuid(FinesseClientHandle);
        req.clientuuid.len = sizeof(uuid_t);
        req.request_case = FINESSE__FINESSE_REQUEST__REQUEST_PATH_SEARCH_REQ;
        req.pathsearchreq = &path_search;

        index = 0;
        while (NULL != Files[index]) {
            index++;
        }
        path_search.n_files = index;

        index = 0;
        while (NULL != Paths[index]) {
            index++;
        }
        path_search.n_paths = index;

        path_search.files = Files;
        path_search.paths = Paths;

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

int FinesseSendPathSearchResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, char *Path, int64_t Result)
{
    Finesse__FinesseResponse rsp = FINESSE__FINESSE_RESPONSE__INIT;
    Finesse__FinesseMessageHeader header = FINESSE__FINESSE_MESSAGE_HEADER__INIT;
    Finesse__FinesseResponse__PathSearch psr = FINESSE__FINESSE_RESPONSE__PATH_SEARCH__INIT;
    void *buffer = NULL;
    size_t buffer_len = 0;
    size_t packed_buffer_len = 0;
    int status = -ENOSYS;

    while (NULL == buffer) {
        finesse_set_server_message_header(FinesseServerHandle, &header, RequestId, FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__PATH_SEARCH);

        rsp.header = &header;
        rsp.status = Result;
        rsp.response_case = FINESSE__FINESSE_RESPONSE__RESPONSE_PATH_SEARCH_RSP;
        rsp.pathsearchrsp = &psr;
        psr.name = Path;

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


int FinesseGetPathSearchResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId, char **Path)
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
        // TODO: allocate a string buffer for the return string
        if (0 == rsp->status) {
            assert(rsp->pathsearchrsp);
            assert(rsp->pathsearchrsp->name);
            *Path = malloc(strlen(rsp->pathsearchrsp->name) + sizeof(char));
            if (NULL == *Path) {
                status = -ENOMEM;
                break;
            }
            strcpy(*Path, rsp->pathsearchrsp->name);
        }

        status = rsp->status;
        break;
    }

    if (NULL != buffer) {
        FinesseFreeClientResponse(FinesseClientHandle, buffer);
        buffer = NULL;
    }

    return status;
}

void FinesseFreePathSearchResponse(finesse_client_handle_t FinesseClientHandle, char *PathToFree)
{
    (void) FinesseClientHandle;
    free(PathToFree);
}

