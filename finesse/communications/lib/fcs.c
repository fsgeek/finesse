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
    int                             client_connection;
    int                             client_shm_fd;
    size_t                          client_shm_size;
    void *                          client_shm;
    int                             aux_shm_fd;
    int                             aux_shm_size;
    void *                          aux_shm;
    char                            aux_shm_path[MAX_SHM_PATH_NAME];
} connection_state_t;

typedef struct server_connection_state {
    int                         server_connection;
    pthread_t                   listener_thread;
    uuid_t                      server_uuid;
    char                        server_connection_name[MAX_SHM_PATH_NAME];
    connection_state_t * client_connection_state_table[SHM_PAGE_COUNT];
} server_connection_state_t;

static void teardown_client_connection(connection_state_t *ccs)
{
    int status;

    assert(NULL != ccs);

    if (ccs->client_connection >= 0) {
        status = close(ccs->client_connection);
        assert(0 == status);
        ccs->client_connection = -1;
    }

    if (ccs->client_shm) {
        status = munmap(ccs->client_shm, ccs->client_shm_size);
        assert(0 == status);
        ccs->client_shm = (void *)0;
        ccs->client_shm_size = 0;
    }

    if (ccs->client_shm_fd >= 0) {
        status = close(ccs->client_shm_fd);
        assert(0 == status);
        ccs->client_shm_fd = -1;
    }

    if (NULL != ccs->aux_shm) {
        status = munmap(ccs->aux_shm, ccs->aux_shm_size);
        assert(0 == status);
        ccs->aux_shm = (void *)0;
        ccs->aux_shm_size = 0;
    }

    if (ccs->aux_shm_fd >= 0) {
        status = close(ccs->aux_shm_fd);
        assert(0 == status);
        ccs->aux_shm_fd = -1;
    }

    free(ccs);
}

static void *listener(void *context) 
{
    server_connection_state_t *scs = (server_connection_state_t *) context;
    int status = 0;
    char buffer[SHM_PAGE_SIZE]; // use for messages
    fincomm_registration_confirmation conf;
    unsigned index;

    assert(NULL != context);
    assert(scs->server_connection >= 0);

    while (scs->server_connection >= 0) {
        connection_state_t *new_client = NULL;
        fincomm_registration_info *reg_info = (fincomm_registration_info *)buffer;
        struct stat stat;
        
        if (NULL == new_client) {
            new_client = (connection_state_t *)malloc(sizeof(connection_state_t));
        }
        assert(NULL != new_client);
        memset(new_client, 0, sizeof(connection_state_t));
        new_client->aux_shm_fd = -1;
        new_client->client_shm_fd = -1;
        new_client->client_connection = -1;

        new_client->client_connection = accept(scs->server_connection, 0, 0);
        assert(new_client->client_connection >= 0);
        status = read(new_client->client_connection, buffer, sizeof(buffer));
        assert(status >= sizeof(fincomm_registration_info));

        // Now we have the registration information
        new_client->reg_info = *reg_info;
        assert(new_client->reg_info.ClientSharedMemPathNameLength < MAX_SHM_PATH_NAME);
        new_client->reg_info.ClientSharedMemPathName[new_client->reg_info.ClientSharedMemPathNameLength] = '\0';
        assert(strlen(new_client->reg_info.ClientSharedMemPathName) == new_client->reg_info.ClientSharedMemPathNameLength);

        // map in the shared memory
        new_client->client_shm_fd = shm_open(new_client->reg_info.ClientSharedMemPathName, O_RDWR, 0600);
        assert(new_client->client_shm_fd >= 0);

        status = fstat(new_client->client_shm_fd, &stat);
        assert(status >= 0);
        assert(stat.st_size >= SHM_PAGE_SIZE); 
        new_client->client_shm_size = stat.st_size;

        new_client->client_shm = mmap(NULL, new_client->client_shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, new_client->client_shm_fd, 0);
        assert(MAP_FAILED != new_client->client_shm);

        // Prepare registration acknowledgment.
        memset(&conf, 0, sizeof(conf));

        conf.Result = 0;
        uuid_copy(conf.ServerId, scs->server_uuid);
        conf.ClientSharedMemSize = new_client->client_shm_size;


        // Insert into the table
        for (index = 0; index < SHM_PAGE_COUNT; index++) {
            if (NULL == scs->client_connection_state_table[index]) {
                scs->client_connection_state_table[index] = new_client;
                break;
            }
        }
        assert(index < SHM_PAGE_COUNT); // otherwise we ran out of space in our table and need to fix this.

        if (SHM_PAGE_COUNT <= index) {
            conf.Result = ENOMEM;
        }

        // Send client response
        status = send(new_client->client_connection, &conf, sizeof(conf), 0);
        assert(sizeof(conf) == status);

        if (0 != conf.Result) {
            teardown_client_connection(new_client);
            new_client = NULL;
        }
    }

    return (void *)0;
}

static int CheckForLiveServer(server_connection_state_t *scs)
{
    int status = 0;
    struct stat scs_stat;
    int client_sock = -1;
    struct sockaddr_un server_addr;

    assert(NULL != scs);
    assert(strlen(scs->server_connection_name) > 0);

    while (NULL != scs) {
        status = stat(scs->server_connection_name, &scs_stat);

        if ((status < 0) && (ENOENT == errno)) {
            status = 0; // 0 = does not exist
            break;
        }

        // let's see if we can connect to it.
        client_sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
        assert(client_sock >= 0);
        server_addr.sun_family = AF_UNIX;
        strncpy(server_addr.sun_path, scs->server_connection_name, sizeof(server_addr.sun_path));

        status = connect(client_sock, &server_addr, sizeof(server_addr));
        if (status < 0) {
            status = unlink(scs->server_connection_name);
            status = 0;
            break; // 0 = does not exist
        }

        // Done
        break;

    }

    if (client_sock >= 0) {
        close(client_sock);
    }
    
    return status;
}

int FinesseStartServerConnection(finesse_server_handle_t *FinesseServerHandle)
{
    int status = 0;
    DIR *dir = NULL;
    server_connection_state_t *scs = NULL;
    struct sockaddr_un server_saddr;


    while (NULL == dir) {

        scs = malloc(sizeof(server_connection_state_t));
        if (NULL == scs) {
            status = ENOMEM;
            break;
        }
        memset(scs, 0, sizeof(server_connection_state_t));
        uuid_generate(scs->server_uuid);

        dir = opendir(FINESSE_SERVICE_PREFIX);

        if ((NULL == dir) && (ENOENT == errno)) {
            status = mkdir(FINESSE_SERVICE_PREFIX, 0700); // only accessible for this user!
            assert(0 == status);
            dir = opendir(FINESSE_SERVICE_PREFIX);
        }

        if (0 != status) {
            break;
        }

        status = GenerateServerName(scs->server_connection_name, sizeof(scs->server_connection_name));
        assert(0 == status);
        
        status = CheckForLiveServer(scs);
        assert(0 == status);

        // need a socket
        scs->server_connection = socket(AF_UNIX, SOCK_SEQPACKET, 0);
        if (scs->server_connection < 0) {
            status = errno;
            break;
        }

        server_saddr.sun_family = AF_UNIX;
        assert(strlen(scs->server_connection_name) < sizeof(server_saddr.sun_path));
        strncpy(server_saddr.sun_path, scs->server_connection_name, sizeof(server_saddr.sun_path));
        status = bind(scs->server_connection, (struct sockaddr *) &server_saddr, sizeof(server_saddr));
        if (status < 0) {
            status = errno;
            break;
        }

        status = listen(scs->server_connection, SHM_PAGE_COUNT);
        assert(status >= 0); // listen shouldn't fail

        status = pthread_create(&scs->listener_thread, NULL, listener, scs);
        assert(0 == status);

        // Done
        break;
    }

    if (NULL != dir) {
        closedir(dir);
    }

    *FinesseServerHandle = scs;

    return status;
}

int FinesseStopServerConnection(finesse_server_handle_t FinesseServerHandle)
{
    server_connection_state_t *scs = (server_connection_state_t *) FinesseServerHandle;
    int status = 0;
    void *result = NULL;

    assert(NULL != FinesseServerHandle);

    if (scs->listener_thread) {
        status = pthread_cancel(scs->listener_thread);
        assert(0 == status);
    }

    status = pthread_join(scs->listener_thread, &result);
    assert(PTHREAD_CANCELED == result);
    assert(0 == status);

    status = close(scs->server_connection);
    assert(0 == status);
    scs->server_connection = -1;

    for (unsigned index = 0; index < SHM_PAGE_COUNT; index++) {
        if (NULL != scs->client_connection_state_table[index]) {
            teardown_client_connection(scs->client_connection_state_table[index]);
            scs->client_connection_state_table[index] = NULL;
        }
    }

    status = unlink(scs->server_connection_name);
    assert(0 == status);

    free(scs);

    return status;
}

#if 0
int FinesseGetRequest(finesse_server_handle_t FinesseServerHandle, void **Request, size_t *RequestLen);
int FinesseSendResponse(finesse_server_handle_t FinesseServerHandle, const uuid_t *ClientUuid, void *Response, size_t ResponseLen);
void FinesseFreeRequest(finesse_server_handle_t FinesseServerHandle, void *Request);

int FinesseStartClientConnection(finesse_client_handle_t *FinesseClientHandle);
int FinesseStopClientConnection(finesse_client_handle_t FinesseClientHandle);
int FinesseSendRequest(finesse_client_handle_t FinesseClientHandle, void *Request, size_t RequestLen);
int FinesseGetClientResponse(finesse_client_handle_t FinesseClientHandle, void **Response, size_t *ResponseLen);
void FinesseFreeClientResponse(finesse_client_handle_t FinesseClientHandle, void *Response);

int FinesseSendTestRequest(finesse_client_handle_t FinesseClientHandle, uint64_t *RequestId);
int FinesseSendTestResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, int64_t Result);
int FinesseGetTestResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId);

int FinesseSendNameMapRequest(finesse_client_handle_t FinesseClientHandle, char *NameToMap, uint64_t *RequestId);
int FinesseSendNameMapResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, uuid_t *MapKey, int64_t Result);
int FinesseGetNameMapResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId, uuid_t *MapKey);

int FinesseSendNameMapReleaseRequest(finesse_client_handle_t FinesseClientHandle, uuid_t *MapKey, uint64_t *RequestId);
int FinesseSendNameMapReleaseResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, int64_t Result);
int FinesseGetNameMapReleaseResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId);

int FinesseSendPathSearchRequest(finesse_client_handle_t FinesseClientHandle, char **Files, char **Paths, uint64_t *RequestId);
int FinesseSendPathSearchResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, char *Path, int64_t Result);
int FinesseGetPathSearchResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId, char **Path);
void FinesseFreePathSearchResponse(finesse_client_handle_t FinesseClientHandle, char *PathToFree);

int FinesseSendDirMapRequest(finesse_client_handle_t FinesseClientHandle, uint64_t *RequestId, uuid_t *Key, char *Path);
int FinesseSendDirMapResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, char *Path, int64_t Result);
int FinesseGetDirMapResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId);

int FinesseSendUnlinkRequest(finesse_client_handle_t FinesseClientHandle, const char *NameToUnlink, uint64_t *RequestId);
int FinesseSendUnlinkResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, int64_t Result);
int FinesseGetUnlinkResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId);

int FinesseSendStatfsRequest(finesse_client_handle_t FinesseClientHandle, const char *path, uint64_t *RequestId);
int FinesseSendStatfsResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, struct statvfs *buf, int64_t Result);
int FinesseGetStatfsResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId, struct statvfs *buf);

int FinesseSendFstatfsRequest(finesse_client_handle_t FinesseClientHandle, fuse_ino_t nodeid, uint64_t *RequestId);
int FinesseSendFstatfsResponse(finesse_server_handle_t FinesseServerHandle, uuid_t *ClientUuid, uint64_t RequestId, struct statvfs *buf, int64_t Result);
int FinesseGetFstatfsResponse(finesse_client_handle_t FinesseClientHandle, uint64_t RequestId, struct statvfs *buf);

void (*finesse_init)(void);
int finesse_check_prefix(const char *pathname);
int finesse_open(const char *pathname, int flags, ...);
int finesse_creat(const char *pathname, mode_t mode);
int finesse_openat(int dirfd, const char *pathname, int flags, ...);
int finesse_close(int fd);
int finesse_unlink(const char *pathname);
int finesse_unlinkat(int dirfd, const char *pathname, int flags);
int finesse_statfs(const char *path, struct statvfs *buf);
int finesse_fstatfs(fuse_ino_t nodeid, struct statvfs *buf);
//int finesse_mkdir(const char *path, mode_t mode);
//int finesse_mkdirat(int fd, const char *path, mode_t mode);
#endif // 0
