/*
 * (C) Copyright 2017 Tony Mason
 * All Rights Reserved
 */

#include "nicfsinternal.h"
#include <niccolum_msg.h>
#include <mqueue.h>
#include <uuid/uuid.h>


// module private variables
static mqd_t server_mqd = (mqd_t)-1;
static mqd_t client_mqd = (mqd_t)-1;
static char client_mqd_name[64];
static char *server_name = "/niccolum";
static ssize_t server_mq_msg_size = -1;
static uuid_t clientUuid;

// forward declarations
static void *alloc_response_buffer(void);

// move to header

//
// return non-zero value
//
static unsigned get_messageid(void) {
    static unsigned message_id = 1;
    unsigned id = __sync_fetch_and_add(&message_id, 1);
    return id ? id : __sync_fetch_and_add(&message_id, 1);
}

int nicfs_set_client_uuid(niccolum_uuid_t *uuid)
{
    // TODO: should we check for the null UUID and fail?
    memcpy(uuid, clientUuid, sizeof(uuid_t));
    return 0;
}

unsigned nicfs_generate_messageid(void) 
{
    return get_messageid();
}

int nicfs_test_server(void) 
{
    niccolum_message_t request;
    niccolum_message_t *response;
    int status = ENOMEM;
    size_t response_length;

    while (ENOMEM == status) {
        memcpy(request.MagicNumber, NICCOLUM_MESSAGE_MAGIC, NICCOLUM_MESSAGE_MAGIC_SIZE);
        assert(sizeof(request.SenderUuid) == sizeof(uuid_t));
        memcpy(&request.SenderUuid, clientUuid, sizeof(uuid_t));
        request.MessageType = NICCOLUM_TEST;
        request.MessageId = get_messageid();

        status = nicfs_call_server(&request, sizeof(niccolum_message_t), (void **)&response, &response_length);
        if (0 > status) {
            break;
        }

        /* validate */
        assert(response_length >= sizeof(niccolum_message_t));
        assert(0 == memcmp(NICCOLUM_MESSAGE_MAGIC, response->MagicNumber, NICCOLUM_MESSAGE_MAGIC_SIZE));
        assert(0 != memcmp(&response->SenderUuid, &clientUuid, sizeof(uuid_t)));
        assert(NICCOLUM_TEST_RESPONSE == response->MessageType);
        assert(response->MessageId == request.MessageId);

        status = 0;
        break;
    }

    if (NULL != response) {
        nicfs_free_response(response);
    }

    return status;
    
}

//
// Given a path name, we attempt to map it to a handle (UUID)
//
int nicfs_map_name(const char *mapfile_name, uuid_t *uuid)
{
    niccolum_message_t *request, *response;
    size_t message_length;
    size_t response_length;
    int status = ENOMEM;

    /* build a name map message */
    message_length = sizeof(niccolum_message_t) + strlen(mapfile_name) + 1;
    request = (niccolum_message_t *)malloc(message_length);

    while (NULL != request) {
        niccolum_name_map_response_t *nmr = NULL;
        
        memcpy(request->MagicNumber, NICCOLUM_MESSAGE_MAGIC, NICCOLUM_MESSAGE_MAGIC_SIZE);
        assert(sizeof(request->SenderUuid) == sizeof(uuid_t));
        memcpy(&request->SenderUuid, clientUuid, sizeof(uuid_t));
        request->MessageType = NICCOLUM_NAME_MAP_REQUEST;
        request->MessageId = get_messageid();
        request->MessageLength = strlen(mapfile_name) + 1;
        strcpy(request->Message, mapfile_name);

        /* send the message */
        status = nicfs_call_server(request, message_length, (void **)&response, &response_length);
        if (0 > status) {
            break;
        }

        /* validate message */
        assert(0 == memcmp(NICCOLUM_MESSAGE_MAGIC, response->MagicNumber, NICCOLUM_MESSAGE_MAGIC_SIZE));
        assert(0 != memcmp(&response->SenderUuid, &clientUuid, sizeof(uuid_t)));
        assert(NICCOLUM_NAME_MAP_RESPONSE == response->MessageType);
        assert(response->MessageId == request->MessageId);

        /* return the uuid */
        nmr = (niccolum_name_map_response_t *)response->Message;
        if (0 != nmr->Status) {
            status = -1;
            errno = nmr->Status;
            break;
        }
        memcpy(uuid, nmr->Key.Key, sizeof(uuid_t));

        status = 0;

        break;
    }

    // cleanup
    if (NULL != request) {
        free(request);
    }

    if (NULL != response) {
        nicfs_free_response(response);
    }

    return status;
}


// TODO: right now this assumes one call, one response.  This will need to move to
// an async model at some point.
int nicfs_call_server(void *request, size_t req_length, void **response, size_t *rsp_length)
{
    int status = ENOMEM;
    struct timespec rsp_wait_time;
    ssize_t bytes_received;

    *response = NULL;
    *rsp_length = 0;

    if (((mqd_t) -1 == server_mqd) || ((mqd_t)-1 == client_mqd)) {
        nicfs_server_open_connection();        
        if (((mqd_t) -1 == server_mqd) || ((mqd_t)-1 == client_mqd)) {
            errno = EBADF;
            return -1;
        }
    }

    *response = alloc_response_buffer();
    while (NULL != *response) {
        status = mq_send(server_mqd, request, req_length, 0);
        if (0 > status) {
            break;
        }

        status = clock_gettime(CLOCK_REALTIME, &rsp_wait_time);
        assert (0 == status);

        rsp_wait_time.tv_sec += 10;
        rsp_wait_time.tv_nsec = 0;
        bytes_received = mq_timedreceive(client_mqd, (char *)*response, server_mq_msg_size, NULL, &rsp_wait_time);
        if (0 > bytes_received) {
            status = -1;
            break;
        }
        status = 0;
        *rsp_length = (size_t) bytes_received;
        break;
    }

    // cleanup
    if (0 != status) {
        if (NULL != response) {
            nicfs_free_response(*response);
            *response = NULL;
            *rsp_length = 0;
        }
        if (status > 0) {
            errno = status;
            status = -1;
        }
    }

    return status;

}

void nicfs_free_response(void *response)
{
    free(response);
}

void nicfs_server_close_connection(void) 
{
    struct mq_attr attr;
    void *buffer;
    long msg_count;
    ssize_t msg_len;

    mq_getattr(client_mqd, &attr);
    assert(0 == attr.mq_curmsgs);
    if (attr.mq_curmsgs > 0) {
        // shouldn't have anything left on the message queue!
        buffer = malloc(attr.mq_msgsize);
        assert(NULL != buffer);
        for(msg_count = 0; msg_count < attr.mq_curmsgs; msg_count++) {
            msg_len = mq_receive(client_mqd, (char *)buffer, attr.mq_msgsize, NULL);
            fprintf(stderr, "drained message of length %zi\n", msg_len);
        }
        free(buffer);
        buffer = NULL;
    }

    mq_close(server_mqd);
    mq_close(client_mqd);
    mq_unlink(client_mqd_name);
    server_mqd = -1;
    client_mqd = -1;
}

void nicfs_server_open_connection(void)
{
    struct mq_attr attr;
    mqd_t mq_rsp = (mqd_t)-1;
    mqd_t mq_req = (mqd_t)-1;
    int status;

    assert(-1 == server_mqd);
   
    uuid_generate_time_safe(clientUuid);
    strncpy(client_mqd_name, "/niccolum_", sizeof(client_mqd_name));
    uuid_unparse_lower(clientUuid, &client_mqd_name[strlen(client_mqd_name)]);

    /* create our response queue */
    attr = (struct mq_attr){
        .mq_flags = 0,
        .mq_maxmsg = 10,
        .mq_msgsize = 512,
        .mq_curmsgs = 0,
    };

    while ((mqd_t)-1 == mq_rsp) {

    	mq_rsp = mq_open(client_mqd_name, O_RDONLY | O_CREAT, 0600, &attr);
        if ((mqd_t)-1 == mq_rsp) {
            break;
        }

        mq_req = mq_open(server_name, O_WRONLY);
        if ((mqd_t)-1 == mq_req) {
            break;
        }

        status = mq_getattr(mq_rsp, &attr);
        if (0 > status) {
            break;
        }

        server_mq_msg_size = attr.mq_msgsize;

        server_mqd = mq_req;
        client_mqd = mq_rsp;
        mq_req = mq_rsp = (mqd_t) -1;

        break;
    }

    // cleanup
    if ((mqd_t) -1 != mq_rsp) {
        mq_close(mq_rsp);
        mq_unlink(client_mqd_name);
    }

    if ((mqd_t) -1 != mq_req) {
        mq_close(mq_req);
    }

}

static void *alloc_response_buffer(void)
{
    if (server_mq_msg_size < 0) {
        errno = EINVAL;
        return NULL;
    }

    return malloc(server_mq_msg_size);
}

