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

#if !defined(make_mask64)
#define make_mask64(index) (((u_int64_t)1)<<index)
#endif


typedef struct server_internal_connection_state {
    int                         server_connection;
    pthread_t                   listener_thread;
    uuid_t                      server_uuid;
    unsigned char               align0[32];
    pthread_mutex_t             monitor_mutex;
    uint64_t                    waiting_client_request_bitmap;
    unsigned char               align1[64-(sizeof(pthread_mutex_t)+sizeof(uint64_t))];
    pthread_cond_t              monitor_cond; // threads monitor for refresh needed
    unsigned char               align2[64-(sizeof(pthread_cond_t))];
    pthread_cond_t              server_cond; // server thread monitors for refresh
    unsigned char               align3[64-(sizeof(pthread_cond_t))];
    char                        server_connection_name[MAX_SHM_PATH_NAME];
    server_connection_state_t * client_server_connection_state_table[SHM_MESSAGE_COUNT];
} server_internal_connection_state_t;

_Static_assert(0 == (offsetof(server_internal_connection_state_t, monitor_mutex) % 64), "Misaligned");
_Static_assert(0 == (offsetof(server_internal_connection_state_t, monitor_cond) % 64), "Misaligned");
_Static_assert(0 == (offsetof(server_internal_connection_state_t, server_connection_name) % 64), "Misaligned");


static void teardown_client_connection(server_connection_state_t *ccs)
{
    int status;

    assert(NULL != ccs);

    if (ccs->monitor_thread_active) {
        status = pthread_cancel(ccs->monitor_thread);
        assert(0 == status);
        // status = pthread_join(ccs->monitor_thread, NULL);
        // assert(ECANCELED == status);
        ccs->monitor_thread_active = 0;
    }

    if (ccs->client_connection >= 0) {
        status = close(ccs->client_connection);
        assert(0 == status);
        ccs->client_connection = -1;
    }

    if (ccs->client_shm) {
        // TODO: we might need to do further cleanup here
        // before unmapping the memory, since there are
        // blocking objects within that memory region.
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

typedef struct _inbound_request_worker_info {
    unsigned index;
    server_internal_connection_state_t *Scs;
} inbound_request_worker_info;

// This worker thread monitors inbound requests from the client
static void *inbound_request_worker(void *context)
{
    inbound_request_worker_info *irwi = (inbound_request_worker_info *)context;
    server_internal_connection_state_t *scs;
    server_connection_state_t *ccs;
    uint64_t pending_bit = make_mask64(irwi->index);
    int status;

    assert(NULL != context);
    scs = irwi->Scs;
    assert(NULL != scs);
    assert(0 == (scs->waiting_client_request_bitmap & pending_bit)); // can't be set yet!
    ccs = scs->client_server_connection_state_table[irwi->index];
    if (NULL == ccs) {
        // Race: this can happen when shutdown gets called before we've finished init.
        // Better fix would be to make startup wait until we're in a "safe" state, but
        // for now, this is good enough.
        return NULL;
    }

    // This is trying to be clever, so it is likely wrong
    // We start with the bit clear (see the assert above)
    // (1) We block until something is waiting
    // (2) We grab the lock and signal the main server loop so it looks
    // (3) The mains server loop can drain requests directly
    // (4) When the main server loop finds no more entries, it can kick us to look again.
    // (5) This thread only holds the lock when setting (or clearing) the bit.
    //
    // note: the SERVER or this monitor CAN clear that bit.  This code should work either
    // way.
    for (;;) {
        // Block until something is available - do NOT hold the lock!
        status = FinesseReadyRequestWait(ccs->client_shm);
        if (ENOTCONN == status) {
            // shutdown;
            break;
        }
        pthread_mutex_lock(&scs->monitor_mutex);
        scs->waiting_client_request_bitmap |= pending_bit; // turn on bit - something waiting
        pthread_cond_signal(&scs->server_cond); // notify server we've turned on a bit
        pthread_cond_wait(&scs->monitor_cond, &scs->monitor_mutex); // wait for server to tell us to look again
        scs->waiting_client_request_bitmap &= ~pending_bit; // turn off bit (we need to check again)
        pthread_mutex_unlock(&scs->monitor_mutex);
    }
    ccs->monitor_thread_active = 0;

    free(irwi);
    irwi = NULL;

    return NULL;
}

static void *listener(void *context) 
{
    server_internal_connection_state_t *scs = (server_internal_connection_state_t *) context;
    int status = 0;
    char buffer[SHM_PAGE_SIZE]; // use for messages
    fincomm_registration_confirmation conf;
    unsigned index;

    assert(NULL != context);
    assert(scs->server_connection >= 0);

    while (scs->server_connection >= 0) {
        server_connection_state_t *new_client = NULL;
        fincomm_registration_info *reg_info = (fincomm_registration_info *)buffer;
        struct stat stat;
        
        if (NULL == new_client) {
            new_client = (server_connection_state_t *)malloc(sizeof(server_connection_state_t));
        }
        assert(NULL != new_client);
        memset(new_client, 0, sizeof(server_connection_state_t));
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

        // initialize the shared memory region
        status = FinesseInitializeMemoryRegion(new_client->client_shm);
        assert(0 == status);

        // Prepare registration acknowledgment.
        memset(&conf, 0, sizeof(conf));

        conf.Result = 0;
        uuid_copy(conf.ServerId, scs->server_uuid);
        conf.ClientSharedMemSize = new_client->client_shm_size;


        // Insert into the table
        for (index = 0; index < SHM_MESSAGE_COUNT; index++) {
            if (NULL == scs->client_server_connection_state_table[index]) {
                inbound_request_worker_info *irwi = (inbound_request_worker_info *)malloc(sizeof(inbound_request_worker_info));
                assert(NULL != irwi);
                irwi->index = index;
                irwi->Scs = scs;
                scs->client_server_connection_state_table[index] = new_client;
                status = pthread_create(&new_client->monitor_thread, NULL, inbound_request_worker, irwi);
                assert(0 == status);
                new_client->monitor_thread_active = 1;
                break;
            }
        }
        assert(index < SHM_MESSAGE_COUNT); // otherwise we ran out of space in our table and need to fix this.

        if (SHM_MESSAGE_COUNT <= index) {
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

static int CheckForLiveServer(server_internal_connection_state_t *scs)
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
    server_internal_connection_state_t *scs = NULL;
    struct sockaddr_un server_saddr;


    while (NULL == dir) {

        scs = malloc(sizeof(server_internal_connection_state_t));
        if (NULL == scs) {
            status = ENOMEM;
            break;
        }
        memset(scs, 0, sizeof(server_internal_connection_state_t));
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

        status = listen(scs->server_connection, SHM_MESSAGE_COUNT);
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
    server_internal_connection_state_t *scs = (server_internal_connection_state_t *) FinesseServerHandle;
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

    for (unsigned index = 0; index < SHM_MESSAGE_COUNT; index++) {
        if (NULL != scs->client_server_connection_state_table[index]) {
            teardown_client_connection(scs->client_server_connection_state_table[index]);
            scs->client_server_connection_state_table[index] = NULL;
        }
    }

    status = unlink(scs->server_connection_name);
    assert(0 == status);

    free(scs);

    return status;
}

int FinesseSendResponse(finesse_server_handle_t FinesseServerHandle, const uuid_t *ClientUuid, void *Response, size_t ResponseLen)
{
    int status = 0;
    server_internal_connection_state_t *scs = (server_internal_connection_state_t *)FinesseServerHandle;

    assert(NULL != scs);
    assert(NULL != ClientUuid);
    assert(NULL != Response);
    assert(0 == ResponseLen);

#if 0
    client_mq_server_connection_state_t *ccs = NULL;

    (void)FinesseServerHandle;

    ccs = get_client_mq_connection(ClientUuid);
    if (NULL == ccs)
    {
        return -EMFILE;
    }

    status = mq_send(ccs->queue_descriptor, Response, ResponseLen, 0);

    release_client_mq_connection(ccs);
#endif // 0

    return status;
}

//
// Local helper function
//
// scans index values in [start,end) for the client connections in the given server connection state
// structure.  The passed in bitmap is also updated.  If a message is found, it is returned as well
// ENOENT = nothing found. Internally, ENOTCONN is handled by removing the (now stale) entry from
// the table.
//
// Yes, this is an ugly subroutine.
// 
static int scan_for_request(unsigned start, unsigned end, server_internal_connection_state_t *scs, uint64_t *bitmap, fincomm_message *message, unsigned *index)
{
    int status = ENOENT;

    *index = end;

    // We need to scan across the connected clients to find a message
    for (unsigned i = start; i < end; i++) {

        // ignore empty table entries
        if (NULL == scs->client_server_connection_state_table[i]) {
            assert(0 == ((*bitmap) & make_mask64(i))); // shouldn't be set!
            continue; // no client
        }

        // if the bit is set, let's see if we can get a message
        if ((*bitmap) & make_mask64(i)) {
            status = FinesseGetReadyRequest(scs->client_server_connection_state_table[i]->client_shm, message);
            if (0 == status) {
                // we found one - capture it and break
                *index = i;
                break;
            }
            if (ENOTCONN == status) {
                // This client has disconnected
                teardown_client_connection(scs->client_server_connection_state_table[i]);
                scs->client_server_connection_state_table[i] = NULL;
                status = ENOENT;
                continue; // try the next client
            }
            assert(ENOENT == status); // otherwise, this logic is broken
            // We clear the bit
            (*bitmap) &= ~make_mask64(i);
            continue; // try the next client
        }
    }
    return status;
}

//
// This gets another request for the Finesse server to process.
//
//  This routine is expected to be called from multiple service threads (though that's not required).
//  It handles requests for all inbound clients to this server (subject to the limit we've coded into
//  this prototype.)
//
int FinesseGetRequest(finesse_server_handle_t FinesseServerHandle, void **Request, size_t *RequestLen)
{
    int status = 0;
    server_internal_connection_state_t *scs = (server_internal_connection_state_t *)FinesseServerHandle;
    fincomm_message message = NULL;
    long int rnd = random() % SHM_MESSAGE_COUNT;
    unsigned index = SHM_MESSAGE_COUNT;
    u_int64_t captured_bitmap = 0;

    assert(NULL != FinesseServerHandle);
    assert(NULL != Request);
    assert(NULL != RequestLen);

    // this operation blocks until it finds a request to return to the caller.
    for(;;) {
        int found = 0;

        // Let's check and see if there are any connected clients at this point
        for (unsigned i = rnd; i < SHM_MESSAGE_COUNT; i++) {
            if (NULL != scs->client_server_connection_state_table[i]) {
                found = 1;
                break;
            }
        }
        if (0 == found) {
            status = ENOTCONN; // there aren't any connected clients!
            break;
        }

        // We're going to wake up the secondary threads, so they can
        // check if there are messages waiting.  I really wish I didn't
        // need secondary threads, but there's no simple way for us to
        // block on multiple condition variables; I'd need to use some
        // other mechanism and for now, this is sufficient.
        // TODO: review if there's not a better way to monitor for state
        // changes without multiple threads.
        captured_bitmap = 0;

        // First thing to do is kick the secondary threads to update
        // state.  Note that if there's already work to do, this
        // won't actually wake up the monitor threads.  If there
        // isn't work to do, it wakes up those threads and asks
        // them to continue.
        //
        // Since the monitor operation is a broadcast, it will
        // get all the threads to run and update their respective
        // bits.  Since the mutex lock is held across the broadcast
        // they can't proceed until after we've gone into the wait
        // on the condition, so this should avoid the lost signal
        // problem. We could broadcast more aggressively, but I
        // don't see a benefit to it.
        //
        // Monitor threads that don't have any activity will block
        // and wait for activity to happen and when it does, they'll
        // update the bitmap.
        // 
        pthread_mutex_lock(&scs->monitor_mutex);
        while (0 == scs->waiting_client_request_bitmap) {
            pthread_cond_broadcast(&scs->monitor_cond); // wake up the monitor threads
            pthread_cond_wait(&scs->server_cond, &scs->monitor_mutex); // wait for a monitor thread wake this thread up
        }
        captured_bitmap = scs->waiting_client_request_bitmap;
        pthread_mutex_unlock(&scs->monitor_mutex);
        assert(0 != captured_bitmap);

        // I randomize the starting point to try and ensure fair
        // servicing of operations.

        // scan across the second part of the range
        status = scan_for_request(rnd, SHM_MESSAGE_COUNT, scs, &captured_bitmap, &message, &index);

        if (index < SHM_MESSAGE_COUNT) {
            // we found one, so we're done
            assert(0 == status);
            break;
        }
        assert(ENOENT == status);

        // scan across the first part of the range
        status = scan_for_request(0, rnd, scs, &captured_bitmap, &message, &index);

        if (index < rnd) {
            // we found one, so we're done
            assert(0 == status);
            break;
        }
        assert(ENOENT == status);

        // At this point we will try again, as perhaps the state of the world has changed for us.

    }

    *Request = message;
    *RequestLen = sizeof(fincomm_message);

    return status;
}

// This doesn't make sense for the server now since there's no allocation
// Plus, if it is a server function, it should be in fcs.c not here.
void FinesseFreeRequest(finesse_server_handle_t FinesseServerHandle, void *Request)
{
    assert(NULL != FinesseServerHandle);
    assert(NULL != Request);

    // Nothing to do now, since we don't allocate memory
    return;
}



#if 0
//
// These are the functions that I previously defined.  Look at implementing them in the new communications
// model - possibly in separate files?
//

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
