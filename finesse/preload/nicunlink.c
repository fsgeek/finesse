/*
 * (C) Copyright 2017 Tony Mason
 * All Rights Reserved
 */


#include "nicfsinternal.h"
#include <niccolum_msg.h>
#include <uuid/uuid.h>

static int nic_unlink_call(const char *unlinkfile_name);

static int nic_unlink(const char *pathname)
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

int nic_unlinkat(int dirfd, const char *pathname, int flags)
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

int nicfs_unlink(const char *pathname)
{
    int status;

    nicfs_init();

    //
    // Let's see if it makes sense for us to try opening this
    //
    status = nic_unlink_call(pathname);

    // TODO: does it make sense to invoke the kernel if this fails for all cases?
    if (0 > status) {
        nic_unlink(pathname);
    }

    return status;
}


static int nic_unlink_call(const char *unlinkfile_name)
{
    niccolum_message_t *request, *response;
    size_t message_length;
    size_t response_length;
    int status = ENOMEM;

    /* build an unlink message */
    message_length = sizeof(niccolum_message_t) + offsetof(niccolum_unlink_request_t, Name) + strlen(unlinkfile_name) + 1;
    request = (niccolum_message_t *)malloc(message_length);

    while (NULL != request) {
        niccolum_unlink_request_t *ulreq = NULL;
        niccolum_unlink_response_t *ulrsp = NULL;
        
        memcpy(request->MagicNumber, NICCOLUM_MESSAGE_MAGIC, NICCOLUM_MESSAGE_MAGIC_SIZE);
        assert(sizeof(request->SenderUuid) == sizeof(uuid_t));
        (void) nicfs_set_client_uuid(&request->SenderUuid);
        request->MessageType = NICCOLUM_UNLINK_REQUEST;
        request->MessageId = nicfs_generate_messageid();

        ulreq = (niccolum_unlink_request_t *) request->Message;
        ulreq->NameLength = strlen(unlinkfile_name);
        strcpy(ulreq->Name, unlinkfile_name);
        
        request->MessageLength = offsetof(niccolum_unlink_request_t, Name) + ulreq->NameLength + 1;

        /* send the message */
        status = nicfs_call_server(request, message_length, (void **)&response, &response_length);
        if (0 > status) {
            break;
        }

        /* validate message */
        assert(0 == memcmp(NICCOLUM_MESSAGE_MAGIC, response->MagicNumber, NICCOLUM_MESSAGE_MAGIC_SIZE));
        assert(NICCOLUM_UNLINK_RESPONSE == response->MessageType);
        assert(response->MessageId == request->MessageId);

        /* return the uuid */
        ulrsp = (niccolum_unlink_response_t *)response->Message;
        if (0 != ulrsp->Status) {
            status = -1;
            errno = ulrsp->Status;
            break;
        }
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
