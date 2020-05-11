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

typedef struct connection_state {
    fincomm_registration_info       reg_info;
    int                             server_connection;
    struct sockaddr_un              server_sockaddr;
    int                             server_shm_fd;
    size_t                          server_shm_size;
    void *                          server_shm;
    int                             aux_shm_fd;
    int                             aux_shm_size;
    void *                          aux_shm;
    char                            aux_shm_path[MAX_SHM_PATH_NAME];
} connection_state_t;

static void CleanupClientConnectionState(connection_state_t *ccs)
{
    int status;

    if (NULL == ccs) {
        return;
    }

    if ((ccs->reg_info.ClientSharedMemPathNameLength > 0) &&
        (strlen(ccs->reg_info.ClientSharedMemPathName) > 0)) {
        (void) unlink(ccs->reg_info.ClientSharedMemPathName);
        ccs->reg_info.ClientSharedMemPathNameLength = 0;
        ccs->reg_info.ClientSharedMemPathName[0] = '\0';
    }

    if (ccs->server_connection >= 0) {
        status = close(ccs->server_connection);
        assert(0 == status);
        ccs->server_connection = -1;
    }

    if ((NULL != ccs->server_shm) &&
        (ccs->server_shm_size > 0)) {
        // Shutdown the shared memory region - note the server may have threads
        // blocked, but this should terminate them so the shared memory region
        // can be disconnected.
        FinesseDestroyMemoryRegion(ccs->server_shm);

        status = munmap(ccs->server_shm, ccs->server_shm_size);
        assert(0 == status);
        ccs->server_shm = NULL;
        ccs->server_shm_size = 0;
    }

    if (ccs->server_shm_fd >= 0) {
        status = close(ccs->server_shm_fd);
        assert(0 == status);
        ccs->server_shm_fd = -1;
    }

    if (ccs->aux_shm_fd >= 0) {
        status = close(ccs->aux_shm_fd);
        assert(0 == status);
        ccs->aux_shm_fd = -1;
    }

    if ((NULL != ccs->aux_shm) && 
        (ccs->aux_shm_size)) {
        status = munmap(ccs->aux_shm, ccs->aux_shm_size);
        assert(0 == status);
        ccs->aux_shm = NULL;
        ccs->aux_shm_size = 0;
    }

    free(ccs);
    ccs = NULL;

}


int FinesseStartClientConnection(finesse_client_handle_t *FinesseClientHandle)
{
    int status = 0;
    connection_state_t *ccs = NULL;
    fincomm_registration_confirmation conf;

    while (0 == status) {
        ccs = (connection_state_t *)malloc(sizeof(connection_state_t));
        if (NULL == ccs) {
            status = ENOMEM;
            break;
        }
        memset(ccs, 0, sizeof(connection_state_t));

        uuid_generate(ccs->reg_info.ClientId);
        status = GenerateClientSharedMemoryName(ccs->reg_info.ClientSharedMemPathName, sizeof(ccs->reg_info.ClientSharedMemPathName), ccs->reg_info.ClientId);
        assert(0 == status);
        ccs->reg_info.ClientSharedMemPathNameLength = strlen(ccs->reg_info.ClientSharedMemPathName);
        assert(ccs->reg_info.ClientSharedMemPathNameLength > 0);

        ccs->server_shm_fd = shm_open(ccs->reg_info.ClientSharedMemPathName, O_RDWR | O_CREAT | O_EXCL, 0600);
        assert(ccs->server_shm_fd >= 0);

        ccs->server_shm_size = sizeof(fincomm_message_block) + (SHM_PAGE_SIZE * SHM_MESSAGE_COUNT);
        status = ftruncate(ccs->server_shm_fd, ccs->server_shm_size);
        assert(0 == status);

        ccs->server_shm = mmap(NULL, ccs->server_shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, ccs->server_shm_fd, 0);
        assert(MAP_FAILED != ccs->server_shm);

        ccs->server_sockaddr.sun_family = AF_UNIX;
        status = GenerateServerName(ccs->server_sockaddr.sun_path, sizeof(ccs->server_sockaddr.sun_path));
        assert(0 == status);

        ccs->server_connection = socket(AF_UNIX, SOCK_SEQPACKET, 0);
        assert(ccs->server_connection >= 0);

        status = connect(ccs->server_connection, &ccs->server_sockaddr, sizeof(ccs->server_sockaddr));
        if (status < 0) {
            break;
        }

        status = send(ccs->server_connection, &ccs->reg_info, sizeof(ccs->reg_info), 0);
        assert(sizeof(ccs->reg_info) == status);

        memset(&conf, 0, sizeof(conf));
        status = recv(ccs->server_connection, &conf, sizeof(conf), 0);
        assert(sizeof(conf) == status);
        assert(conf.ClientSharedMemSize == ccs->server_shm_size);
        assert(0 == conf.Result);

        // Unlink shared memory so it will "go away" when the client/server go away
        status = shm_unlink(ccs->reg_info.ClientSharedMemPathName);
        assert(0 == status);

        // Done!
        break;

    }

    if (0 != status) {
        CleanupClientConnectionState(ccs);
        ccs = NULL;
    }

    *FinesseClientHandle = ccs;
    return status;
}

int FinesseStopClientConnection(finesse_client_handle_t FinesseClientHandle)
{
    int status = 0;
    connection_state_t *ccs = NULL;

    CleanupClientConnectionState(ccs);

    return status;
}


void FinesseFreeRequest(finesse_server_handle_t FinesseServerHandle, void *Request)
{
    assert(NULL != FinesseServerHandle);
    assert(NULL != Request);

#if 0
    (void)FinesseServerHandle;
    if (NULL != Request)
    {
        free(Request);
    }
#endif // 0

}

#if 0
int FinesseStartClientConnection(finesse_client_handle_t *FinesseClientHandle)
{
    int status = ENOMEM;
    connection_state_t *client_handle = NULL;
    size_t allocsize = 0;
    size_t queue_name_length = 0;

    queue_name_length = strlen(mq_prefix) + 37; // prefix + uuid size + NULL
    allocsize = offsetof(connection_state_t, queue_name) + queue_name_length;

    client_handle = (connection_state_t *)malloc(allocsize);

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
    connection_state_t *client_handle = (connection_state_t *)FinesseClientHandle;

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
