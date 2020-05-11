/*
 * (C) Copyright 2017-2020 Tony Mason
 * All Rights Reserved
*/
#include <finesse.h>
#include "fincomm.h"

static fincomm_shared_memory_region *CreateInMemoryRegion(void)
{
    int status;
    fincomm_shared_memory_region *fsmr = NULL;


}

//
// This is the new communications lib implementations of these APIs
//

int FinesseGetRequest(finesse_server_handle_t FinesseServerHandle, void **Request, size_t *RequestLen)
{
    void *buffer = NULL;
    size_t buffer_len = FINESSE_MQ_MAX_MESSAGESIZE;
    int status = -ENOMEM;
    server_connection_state_t *server_handle = (server_connection_state_t *)FinesseServerHandle;

    assert(NULL != server_handle);
    assert(((mqd_t)-1) != server_handle->server_queue);
    assert(buffer_len > 0);
    buffer = malloc(buffer_len);

    while (NULL != buffer)
    {
        ssize_t bytes_received;

        bytes_received = mq_receive(server_handle->server_queue, (char *)buffer, buffer_len, NULL);
        if (0 > bytes_received)
        {
            status = -1;
            break;
        }

        assert(buffer_len >= (size_t)bytes_received);

        *Request = buffer;
        *RequestLen = bytes_received;

        buffer = NULL;

        status = 0;
        break;
    }

    if (NULL != buffer)
    {
        free(buffer);
        buffer = NULL;
    }

    return status;
}

int FinesseSendResponse(finesse_server_handle_t FinesseServerHandle, const uuid_t *ClientUuid, void *Response, size_t ResponseLen)
{
    client_mq_connection_state_t *ccs = NULL;
    int status;

    (void)FinesseServerHandle;

    ccs = get_client_mq_connection(ClientUuid);
    if (NULL == ccs)
    {
        return -EMFILE;
    }

    status = mq_send(ccs->queue_descriptor, Response, ResponseLen, 0);

    release_client_mq_connection(ccs);

    return status;
}

void FinesseFreeRequest(finesse_server_handle_t FinesseServerHandle, void *Request)
{
    (void)FinesseServerHandle;
    if (NULL != Request)
    {
        free(Request);
    }
}

