/*
 * (C) Copyright 2020 Tony Mason
 * All Rights Reserved
*/

#include "fincomm.h"
#include <finesse.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <dirent.h>
#include <sys/mman.h>

int FinesseStartClientConnection(finesse_client_handle_t *FinesseClientHandle)
{
    int status = 0;
    *FinesseClientHandle = 0;

    return status;
}

int FinesseStopClientConnection(finesse_client_handle_t FinesseClientHandle)
{
    int status = 0;
    (void) FinesseClientHandle;

    return status;
}

#if 0
int FinesseStartClientConnection(finesse_client_handle_t *FinesseClientHandle)
{
    int status = ENOMEM;
    client_connection_state_t *client_handle = NULL;
    size_t allocsize = 0;
    size_t queue_name_length = 0;

    queue_name_length = strlen(mq_prefix) + 37; // prefix + uuid size + NULL
    allocsize = offsetof(client_connection_state_t, queue_name) + queue_name_length;

    client_handle = (client_connection_state_t *)malloc(allocsize);

    while (NULL != client_handle)
    {
        client_handle->client_queue = (mqd_t)-1;
        client_handle->server_queue = (mqd_t)-1;
        uuid_generate_time_safe(client_handle->client_uuid);
        client_handle->client_request_number = 1;
        strncpy(client_handle->queue_name, mq_prefix, queue_name_length);
        uuid_unparse_lower(client_handle->client_uuid, &client_handle->queue_name[strlen(mq_prefix)]);

        // open server
        client_handle->server_queue = mq_open(MQ_PREFIX, O_WRONLY);

        if (client_handle->server_queue < 0)
        {
            status = errno;
            break;
        }

        client_handle->client_queue = mq_open(client_handle->queue_name, O_RDONLY | O_CREAT, 0622, &finesse_mq_attr);

        if (client_handle->client_queue < 0)
        {
            status = errno;
            break;
        }

        *FinesseClientHandle = client_handle;
        client_handle = NULL;
        status = 0;
        break;
    }

    if (NULL != client_handle)
    {
        if (((mqd_t)-1) != client_handle->client_queue)
        {
            if (0 == mq_close(client_handle->client_queue))
            {
                (void)mq_unlink(client_handle->queue_name);
            }
        }
        if (((mqd_t)-1) != client_handle->server_queue)
        {
            (void)mq_close(client_handle->client_queue);
        }
        free(client_handle);
        client_handle = NULL;
    }

    return status;
}

int FinesseStopClientConnection(finesse_client_handle_t FinesseClientHandle)
{
    int status = 0;
    client_connection_state_t *client_handle = (client_connection_state_t *)FinesseClientHandle;

    while (NULL != client_handle)
    {
        if (0 == mq_close(client_handle->client_queue))
        {
            status = mq_unlink(client_handle->queue_name);
        }

        (void)mq_close(client_handle->server_queue);
        break;
    }

    if (NULL != client_handle)
    {
        free(client_handle);
        client_handle = NULL;
    }

    return status;
}
#endif // 0
