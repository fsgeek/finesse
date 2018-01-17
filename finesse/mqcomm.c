/*
 * (C) Copyright 2017 Tony Mason
 * All Rights Reserved
*/

#define _GNU_SOURCE
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <aio.h>
#include <mqueue.h>
#include <stddef.h>
#include <pthread.h>

#include "finesse.h"
#include "mqcomm.h"
#include "list.h"

#define MQ_PREFIX "/finesse"
static const char *mq_prefix = MQ_PREFIX "_";
static const struct mq_attr finesse_mq_attr = {
    .mq_flags = 0,
    .mq_maxmsg = 10,
    .mq_msgsize = FINESSE_MQ_MAX_MESSAGESIZE,
    .mq_curmsgs = 0,
};

#if !defined(FINESSE_MQ_MAX_WAIT_SECONDS)
#define FINESSE_MQ_MAX_WAIT_SECONDS (10)
#endif

//
// There is a fairly severe limit to the number of open message
// queues by default.  While it is possible to reconfigure things
// for now I just limit the number of simultaneous clients that
// we can have open.
//
// The longer term solution is to move to a different message
// passing paradigm.  Fast Fly-weight Deletation might work
// well, but for now this remains TODO.
//
#if !defined(FINESSE_MQ_MAX_OPEN_CLIENT)
#define FINESSE_MQ_MAX_OPEN_CLIENT (4)
#endif

typedef struct _client_mq_connection_state
{
    mqd_t queue_descriptor; // -1 = not in use
    uuid_t client_uuid;     // UUID mapping to this location
    uint32_t refcount;
    struct timespec last_used; // for recyling
} client_mq_connection_state_t;

client_mq_connection_state_t client_connection_table[FINESSE_MQ_MAX_OPEN_CLIENT];

static client_mq_connection_state_t *get_client_mq_connection(const uuid_t *ClientUuid);
static void release_client_mq_connection(client_mq_connection_state_t *Connection);

// TODO: add a way to clean up MQ state _other than_ by exiting

typedef struct server_connection_state_t
{
    mqd_t server_queue;
} server_connection_state_t;

typedef struct client_connection_state_t
{
    mqd_t client_queue;
    mqd_t server_queue;
    uuid_t client_uuid;
    uint64_t client_request_number;
    struct list pending_responses;
    char queue_name[1];
} client_connection_state_t;

#define CLIENT_PENDING_RESPONSE_CANARY_SIZE (16)
typedef struct client_pending_response_t
{
    char canary[CLIENT_PENDING_RESPONSE_CANARY_SIZE];
    struct list list_entry;
    void *response;
} client_pending_response_t;

const char client_pending_response_canary[CLIENT_PENDING_RESPONSE_CANARY_SIZE] = "0123456789ABCDEF";

uuid_t *finesse_get_client_uuid(finesse_client_handle_t *FinesseClientHandle)
{
    client_connection_state_t *client_handle = (client_connection_state_t *)FinesseClientHandle;

    if (NULL != client_handle)
    {
        return &client_handle->client_uuid;
    }
    return NULL;
}

uint32_t finesse_get_request_id(finesse_client_handle_t *FinesseClientHandle)
{
    client_connection_state_t *client_handle = (client_connection_state_t *)FinesseClientHandle;
    unsigned id = __sync_fetch_and_add(&client_handle->client_request_number, 1);
    while (0 == id)
    {
        __sync_fetch_and_add(&client_handle->client_request_number, 1);
    }
    return id;
}

size_t finesse_get_max_message_size(finesse_client_handle_t *FinesseClientHandle)
{
    (void)FinesseClientHandle; // not used presently
    return FINESSE_MQ_MAX_MESSAGESIZE;
}

void finesse_set_client_message_header(finesse_client_handle_t *FinesseClientHandle,
                                       Finesse__FinesseMessageHeader *Header,
                                       Finesse__FinesseMessageHeader__Operation Operation)
{
    Header->protocolname = (char *)(uintptr_t) "FINESSE";
    Header->messageid = finesse_get_request_id(FinesseClientHandle);
    Header->op = Operation;
}

void finesse_set_client_request_uuid(finesse_client_handle_t *FinesseClientHandle, Finesse__FinesseRequest *Request)
{
    Request->clientuuid.data = (uint8_t *)finesse_get_client_uuid(FinesseClientHandle);
    Request->clientuuid.len = sizeof(uuid_t);
}

void finesse_set_server_message_header(finesse_server_handle_t *FinesseServerHandle,
                                       Finesse__FinesseMessageHeader *Header,
                                       uint64_t MessageId,
                                       Finesse__FinesseMessageHeader__Operation Operation)
{
    (void)FinesseServerHandle;
    Header->protocolname = (char *)(uintptr_t)"FINESSE";
    Header->messageid = MessageId;
    Header->op = Operation;
}

int FinesseStartServerConnection(finesse_server_handle_t *FinesseServerHandle)
{
    int status = ENOMEM;
    server_connection_state_t *server_handle = (server_connection_state_t *)malloc(sizeof(server_connection_state_t));

    *FinesseServerHandle = NULL;

    while (NULL != server_handle)
    {

        server_handle->server_queue = mq_open(MQ_PREFIX, O_RDONLY | O_CREAT, 0622, &finesse_mq_attr);

        if (server_handle->server_queue < 0)
        {
            status = errno;
            break;
        }

        *FinesseServerHandle = (finesse_server_handle_t *)server_handle;
        server_handle = NULL;
        status = 0;

        break;
    }

    if (NULL != server_handle)
    {
        free(server_handle);
    }

    return status;
}

int FinesseStopServerConnection(finesse_server_handle_t FinesseServerHandle)
{
    int status = 0;
    server_connection_state_t *server_handle = (server_connection_state_t *)FinesseServerHandle;

    while (NULL != server_handle)
    {
        if (0 == mq_close(server_handle->server_queue))
        {
            status = mq_unlink(MQ_PREFIX);
        }

        break;
    }

    if (NULL != server_handle)
    {
        free(server_handle);
        server_handle = NULL;
    }

    return status;
}

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

int FinesseSendRequest(finesse_client_handle_t FinesseClientHandle, void *Request, size_t RequestLen)
{
    client_connection_state_t *client_handle = (client_connection_state_t *)FinesseClientHandle;

    assert(NULL != FinesseClientHandle);
    assert(((mqd_t)-1) != client_handle->server_queue);

    return mq_send(client_handle->server_queue, Request, RequestLen, 0);
}

//
// TODO: this needs to be enhanced so that it can handle multiple responses coming back from the server.
//
int FinesseGetClientResponse(finesse_client_handle_t FinesseClientHandle, void **Response, size_t *ResponseLen)
{
    void *buffer = NULL;
    size_t buffer_len = FINESSE_MQ_MAX_MESSAGESIZE;
    int status = -ENOMEM;
    client_connection_state_t *client_handle = (client_connection_state_t *)FinesseClientHandle;

    assert(NULL != client_handle);
    assert(((mqd_t)-1) != client_handle->client_queue);
    assert(buffer_len > 0);
    buffer = malloc(buffer_len);

    while (NULL != buffer)
    {
        struct timespec rsp_wait_time;
        ssize_t bytes_received;

        status = clock_gettime(CLOCK_REALTIME, &rsp_wait_time);
        assert(0 == status);
        if (0 > status)
        {
            break;
        }

        rsp_wait_time.tv_sec += FINESSE_MQ_MAX_WAIT_SECONDS;
        rsp_wait_time.tv_nsec = 0;
        bytes_received = mq_timedreceive(client_handle->client_queue, (char *)buffer, buffer_len, NULL, &rsp_wait_time);
        if (0 > bytes_received)
        {
            status = -1;
            break;
        }

        assert(buffer_len >= (size_t)bytes_received);

        *Response = buffer;
        *ResponseLen = bytes_received;
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

void FinesseFreeClientResponse(finesse_client_handle_t FinesseClientHandle, void *Response)
{
    (void)FinesseClientHandle;
    free(Response);
}

static mqd_t open_client_mq_connection(const uuid_t *ClientUuid)
{
    // use this entry
    mqd_t mqd = (mqd_t)-1;
    size_t connection_name_length;
    char *connection_name = NULL;

    connection_name_length = strlen(mq_prefix) + 37; // prefix + uuid + NULL
    connection_name = malloc(connection_name_length);

    while (NULL != connection_name)
    {

        strncpy(connection_name, mq_prefix, connection_name_length);
        uuid_unparse_lower(*ClientUuid, &connection_name[strlen(mq_prefix)]);

        mqd = mq_open(connection_name, O_WRONLY);

        break;
    }

    if (NULL != connection_name)
    {
        free(connection_name);
        connection_name = NULL;
    }

    return mqd;
}

static client_mq_connection_state_t *get_client_mq_connection(const uuid_t *ClientUuid)
{
    static pthread_rwlock_t lock = PTHREAD_RWLOCK_INITIALIZER;
    static client_mq_connection_state_t *connection = NULL;
    unsigned index, oldest_index = FINESSE_MQ_MAX_OPEN_CLIENT;
    unsigned unused_index = FINESSE_MQ_MAX_OPEN_CLIENT;
    struct timespec oldest;
    int status;

    status = clock_gettime(CLOCK_REALTIME, &oldest);
    assert(0 == status);
    if (0 > status)
    {
        // set to the biggest numger we can
        oldest.tv_sec = ~0;
        oldest.tv_nsec = ~0;
    }

    // look first
    pthread_rwlock_wrlock(&lock);
    while (NULL == connection)
    {
        for (index = 0; index < FINESSE_MQ_MAX_OPEN_CLIENT; index++)
        {

            // first see if the entry is even in use
            if (((mqd_t)-1) == client_connection_table[index].queue_descriptor)
            {
                unused_index = index;
                continue;
            }

            // now see if the uuid matches
            if (0 == uuid_compare(*ClientUuid, client_connection_table[index].client_uuid))
            {
                struct timespec now;

                connection = &client_connection_table[index];
                connection->refcount++;

                status = clock_gettime(CLOCK_REALTIME, &now);

                if (0 == status)
                {
                    client_connection_table[index].last_used = now;
                }

                break;
            }

            // finally, see if this unused entry is older than what we've previously seen
            if ((0 == client_connection_table[index].refcount) &&
                ((client_connection_table[index].last_used.tv_sec < oldest.tv_sec) ||
                 ((client_connection_table[index].last_used.tv_sec == oldest.tv_sec) &&
                  (client_connection_table[index].last_used.tv_nsec < oldest.tv_nsec))))
            {
                oldest = client_connection_table[index].last_used;
                oldest_index = index;
            }
        }

        if (NULL != connection)
        {
            // found an entry
            break;
        }

        if (unused_index >= FINESSE_MQ_MAX_OPEN_CLIENT)
        {
            // is there an old entry we can use?
            if (oldest_index >= FINESSE_MQ_MAX_OPEN_CLIENT)
            {
                // nope
                break;
            }

            (void)mq_close(client_connection_table[oldest_index].queue_descriptor);
            client_connection_table[oldest_index].queue_descriptor = (mqd_t)-1;
            unused_index = oldest_index;
        }

        // didn't find an existing entry, need to set one up
        if (unused_index < FINESSE_MQ_MAX_OPEN_CLIENT)
        {
            // use this entry
            status = clock_gettime(CLOCK_REALTIME, &client_connection_table[index].last_used);
            if (0 > status)
            {
                break;
            }

            client_connection_table[unused_index].queue_descriptor = open_client_mq_connection(ClientUuid);
            if (((mqd_t)-1) == client_connection_table[unused_index].queue_descriptor)
            {
                break;
            }
            memcpy(&client_connection_table[unused_index].client_uuid, *ClientUuid, sizeof(uuid_t));
            client_connection_table[unused_index].refcount = 1;
            connection = &client_connection_table[unused_index];
            break;
        }

        break;
    }

    pthread_rwlock_unlock(&lock);

    return connection;
}

static void release_client_mq_connection(client_mq_connection_state_t *Connection)
{
    assert(NULL != Connection);
}
