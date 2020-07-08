/*
 * (C) Copyright 2020 Tony Mason
 * All Rights Reserved
*/

#include "fcinternal.h"
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
    int                         shutdown;
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


static void create_aux_shm(server_connection_state_t *ccs, unsigned Index)
{
    int status;

    assert(NULL != ccs);
    assert(Index < SHM_MESSAGE_COUNT);

    assert(-1 == ccs->aux_shm_table[Index].AuxShmFd);
    assert(uuid_is_null(ccs->aux_shm_table[Index].AuxShmKey));
    assert(MAP_FAILED == ccs->aux_shm_table[Index].AuxShmMap);
    assert(0 == ccs->aux_shm_table[Index].AuxShmSize);

    // generate a name
    uuid_generate(ccs->aux_shm_table[Index].AuxShmKey);
    status = GenerateClientSharedMemoryName(ccs->aux_shm_table[Index].AuxShmName, MAX_SHM_PATH_NAME, ccs->aux_shm_table[Index].AuxShmKey);
    assert(0 == status);

    // create shared memory
    ccs->aux_shm_table[Index].AuxShmFd = shm_open(ccs->aux_shm_table[Index].AuxShmName, O_RDWR | O_CREAT | O_EXCL, 0600);
    assert(ccs->aux_shm_table[Index].AuxShmFd >= 0);

    ccs->aux_shm_table[Index].AuxShmSize = 1024 * 1024; // hard code for now
    status = ftruncate(ccs->aux_shm_table[Index].AuxShmFd, ccs->aux_shm_table[Index].AuxShmSize);
    assert(0 == status);

    ccs->aux_shm_table[Index].AuxShmMap = mmap(NULL, ccs->aux_shm_table[Index].AuxShmSize, PROT_READ | PROT_WRITE, MAP_SHARED, ccs->aux_shm_table[Index].AuxShmSize, 0);
    assert(MAP_FAILED != ccs->aux_shm_table[Index].AuxShmMap);

}

static void init_aux_shm(server_connection_state_t *ccs)
{
    for(unsigned index = 0; index < SHM_MESSAGE_COUNT; index++) {
        ccs->aux_shm_table[index].AuxShmFd = -1;
        memset(&ccs->aux_shm_table[index].AuxShmKey, 0, sizeof(uuid_t));
        ccs->aux_shm_table[index].AuxShmMap = MAP_FAILED;
        ccs->aux_shm_table[index].AuxShmSize = 0;
        ccs->aux_shm_table[index].AuxInUse = 0;
    }
}

static void destroy_aux_shm(server_connection_state_t *ccs, unsigned Index)
{
    int status;

    // unmap
    status = munmap(ccs->aux_shm_table[Index].AuxShmMap, ccs->aux_shm_table[Index].AuxShmSize);
    assert(0 == status);

    ccs->aux_shm_table[Index].AuxShmSize = 0;
    ccs->aux_shm_table[Index].AuxShmMap = MAP_FAILED;

    // close map file
    if (ccs->aux_shm_table[Index].AuxShmFd >= 0) {
        status = close(ccs->aux_shm_table[Index].AuxShmFd);
        assert(0 == status);
        ccs->aux_shm_table[Index].AuxShmFd = -1;
    }

    // unlink the shared memory
    status = shm_unlink(ccs->aux_shm_table[Index].AuxShmName);
    assert(0 == status);

}

static void shutdown_aux_shm(server_connection_state_t *ccs)
{
    for(unsigned index = 0; index < SHM_MESSAGE_COUNT; index++) {
        if (-1 != ccs->aux_shm_table[index].AuxShmFd) {
            destroy_aux_shm(ccs, index);
        }
    }
}

static void teardown_client_connection(server_connection_state_t *ccs)
{
    int status;

    assert(NULL != ccs);

    if (ccs->monitor_thread_active) {
        status = pthread_cancel(ccs->monitor_thread);
        assert(0 == status);
        (void) pthread_join(ccs->monitor_thread, NULL);
        assert(0 == status);
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

    shutdown_aux_shm(ccs);

    free(ccs);
}

static void listener_cleanup(void *arg)
{
    if (NULL != arg) {
        free(arg);
    }
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
    unsigned locked_count = 0;

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
    pthread_cleanup_push(listener_cleanup, irwi);
    for (;;) {
        // Block until something is available - do NOT hold the lock!
        status = FinesseReadyRequestWait(ccs->client_shm);
        if (ENOTCONN == status) {
            // shutdown;
            break;
        }
        locked_count++; // debug
        pthread_mutex_lock(&scs->monitor_mutex);
        scs->waiting_client_request_bitmap |= pending_bit; // turn on bit - something waiting
        pthread_cond_signal(&scs->server_cond); // notify server we've turned on a bit
        pthread_cond_wait(&scs->monitor_cond, &scs->monitor_mutex); // wait for server to tell us to look again
        scs->waiting_client_request_bitmap &= ~pending_bit; // turn off bit (we need to check again)
        pthread_mutex_unlock(&scs->monitor_mutex);
    }
    ccs->monitor_thread_active = 0;
    pthread_cleanup_pop(0);

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
        int new_client_fd = -1;

        new_client_fd = accept(scs->server_connection, 0, 0);

        if (NULL == new_client) {
            new_client = (server_connection_state_t *)malloc(sizeof(server_connection_state_t));
        }

        assert(NULL != new_client);
        memset(new_client, 0, sizeof(server_connection_state_t));
        new_client->client_shm_fd = -1;
        new_client->client_connection = new_client_fd;

        if (scs->shutdown) {
            // don't care about errors, we're done.
            if (NULL != new_client) {
                free(new_client);
                new_client = NULL;
            }
            break;
        }
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

        // set up the aux shm area
        init_aux_shm(new_client);

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

        // Notify any waiting server thread(s) that the state of the world has changed
        status = pthread_cond_broadcast(&scs->server_cond);
        assert(0 == status);
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
        assert(strlen(scs->server_connection_name) < sizeof(server_addr.sun_path));
        strcpy(server_addr.sun_path, scs->server_connection_name);

        status = connect(client_sock, &server_addr, sizeof(server_addr));
        if (status < 0) {
            (void)unlink(scs->server_connection_name);
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

int FinesseStartServerConnection(const char *MountPoint, finesse_server_handle_t *FinesseServerHandle)
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

        status = GenerateServerName(MountPoint, scs->server_connection_name, sizeof(scs->server_connection_name));
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

    scs->shutdown = 1;

    // Tell any waiting service callers that the state of teh world has changed
    pthread_cond_broadcast(&scs->server_cond);

    // Close the client registration connection - this should kill the listener
    status = close(scs->server_connection);
    assert(0 == status);
    scs->server_connection = -1;

    if (scs->listener_thread) {
        status = pthread_cancel(scs->listener_thread);
        assert(0 == status);
    }

    status = pthread_join(scs->listener_thread, &result);
    assert(PTHREAD_CANCELED == result);
    assert(0 == status);


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

int FinesseSendResponse(finesse_server_handle_t FinesseServerHandle, void *Client, void *Response)
{
    int status = 0;
    server_internal_connection_state_t *sics = (server_internal_connection_state_t *)FinesseServerHandle;
    unsigned index = (unsigned)(uintptr_t)Client; //
    server_connection_state_t *scs = NULL;

    assert(NULL != sics);
    assert(NULL != Response);
    assert(index < SHM_MESSAGE_COUNT);
    scs = sics->client_server_connection_state_table[index];

    if (NULL == scs) {
        // client has disconnected.
        status = ENOTCONN;
    }
    else {
        FinesseResponseReady(scs->client_shm, Response, 0);
        status = 0;
    }

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
int FinesseGetRequest(finesse_server_handle_t FinesseServerHandle, void **Client,  fincomm_message *Request)
{
    int status = 0;
    server_internal_connection_state_t *scs = (server_internal_connection_state_t *)FinesseServerHandle;
    client_connection_state_t *ccs = NULL;
    fincomm_message message = NULL;
    long int rnd = random() % SHM_MESSAGE_COUNT;
    unsigned index = SHM_MESSAGE_COUNT;
    u_int64_t captured_bitmap = 0;

    assert(NULL != FinesseServerHandle);
    assert(NULL != Request);

    // this operation blocks until it finds a request to return to the caller.
    for(;;) {
        int found = 0;

        pthread_mutex_lock(&scs->monitor_mutex);
        captured_bitmap = scs->waiting_client_request_bitmap;
        //
        // The server service thread is going to need to wait if:
        //  (1) there are no clients;
        //  (2) there are clients, but they aren't asking for any work
        //
        while ((0 == found) || (0 == captured_bitmap)) {
            // Let's check and see if there are any connected clients at this point
            for (unsigned i = 0; i < SHM_MESSAGE_COUNT; i++) {
                if (NULL != scs->client_server_connection_state_table[i]) {
                    found = 1;
                    break;
                }
            }

            if (0 != found) {
                // We're going to wake up the secondary threads, so they can
                // check if there are messages waiting.  I really wish I didn't
                // need secondary threads, but there's no simple way for us to
                // block on multiple condition variables; I'd need to use some
                // other mechanism and for now, this is sufficient.
                // TODO: review if there's not a better way to monitor for state
                // changes without multiple threads.
                pthread_cond_broadcast(&scs->monitor_cond); // wake up the monitor threads
            }

            // wait for state change.  Note that this means we need to signal this condition
            // when: (1) a new client connects (found state changes) or (2) someone notices
            // there are messages waiting to be processed.
            pthread_cond_wait(&scs->server_cond, &scs->monitor_mutex); // wait for a monitor thread wake this thread up

            // the world has changed, so we start back up at the top.
            captured_bitmap = scs->waiting_client_request_bitmap;

        }
        pthread_mutex_unlock(&scs->monitor_mutex);

        // Was a shutdown requested?
        if (scs->shutdown) {
            break;
        }

        // We should never be here without a captured bitmap
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

    if (scs->shutdown) {
        *Client = NULL;
        *Request = NULL;
        return ESHUTDOWN;
    }

    *Client = ccs;
    *Request = message;
    if (NULL == Request) {
        *Client = (void *)SHM_MESSAGE_COUNT;
    }
    else {
        *Client = (void *)(uintptr_t)index;
    }
    return status;
}

fincomm_shared_memory_region *FcGetSharedMemoryRegion(finesse_server_handle_t ServerHandle, unsigned Index)
{
    server_internal_connection_state_t *scs = (server_internal_connection_state_t *)ServerHandle;
    assert(NULL != scs);
    assert(Index < SHM_MESSAGE_COUNT);
    assert(NULL != scs->client_server_connection_state_table[Index]);
    return (fincomm_shared_memory_region *)scs->client_server_connection_state_table[Index]->client_shm;
}

const char *fincomm_get_aux_shm_name(finesse_server_handle_t ServerHandle, unsigned ClientIndex, unsigned MessageIndex)
{
    server_internal_connection_state_t *sics = (server_internal_connection_state_t *)ServerHandle;
    server_connection_state_t *scs = NULL;

    assert(NULL != sics);
    scs = sics->client_server_connection_state_table[ClientIndex];
    assert(NULL != scs);

    assert(1 == scs->aux_shm_table[MessageIndex].AuxInUse);

    if (MAP_FAILED == scs->aux_shm_table[MessageIndex].AuxShmMap) {
        // Not allocated yet, so let's do so...
        create_aux_shm(scs, MessageIndex);
        assert(MAP_FAILED != scs->aux_shm_table[MessageIndex].AuxShmMap);
    }

    return scs->aux_shm_table[MessageIndex].AuxShmName;
}


void *fincomm_get_aux_shm(finesse_server_handle_t ServerHandle, unsigned ClientIndex, unsigned MessageIndex, size_t *Size)
{
    server_internal_connection_state_t *sics = (server_internal_connection_state_t *)ServerHandle;
    server_connection_state_t *scs = NULL;

    assert(NULL != sics);
    scs = sics->client_server_connection_state_table[ClientIndex];
    assert(NULL != scs);

    assert(0 == scs->aux_shm_table[MessageIndex].AuxInUse);

    if (MAP_FAILED == scs->aux_shm_table[MessageIndex].AuxShmMap) {
        // Not allocated yet, so let's do so...
        create_aux_shm(scs, MessageIndex);
        assert(MAP_FAILED != scs->aux_shm_table[MessageIndex].AuxShmMap);
    }

    scs->aux_shm_table[MessageIndex].AuxInUse = 1;
    assert(*Size <= scs->aux_shm_table[MessageIndex].AuxShmSize); // otherwise, it's not big enough
    *Size = scs->aux_shm_table[MessageIndex].AuxShmSize;

    return scs->aux_shm_table[MessageIndex].AuxShmMap;
}

void fincomm_release_aux_shm(finesse_server_handle_t ServerHandle, unsigned ClientIndex, unsigned MessageIndex)
{
    server_internal_connection_state_t *sics = (server_internal_connection_state_t *)ServerHandle;
    server_connection_state_t *scs = NULL;

    assert(NULL != sics);
    scs = sics->client_server_connection_state_table[ClientIndex];
    assert(NULL != scs);

    assert(1 == scs->aux_shm_table[MessageIndex].AuxInUse);

    // TODO: cleanup
    scs->aux_shm_table[MessageIndex].AuxInUse = 0;
}

int FinesseGetMessageAuxBuffer(
    finesse_server_handle_t FinesseServerHandle,
    void *Client,
    void *Message,
    void **Buffer,
    size_t *BufferSize)
{
    unsigned index;
    server_internal_connection_state_t *scs = (server_internal_connection_state_t *)FinesseServerHandle;
    unsigned messageIndex;

    for (index = 0; index < SHM_MESSAGE_COUNT; index++) {
        if (scs->client_server_connection_state_table[index] == Client) {
            break;
        }
    }

    assert(index < SHM_MESSAGE_COUNT);

    // TODO: we could use the memory inside the message itself, if there is space

    messageIndex = (unsigned)((((uintptr_t)Message - (uintptr_t)scs->client_server_connection_state_table[index]->client_shm)/SHM_PAGE_SIZE)-1);
    *Buffer = fincomm_get_aux_shm(FinesseServerHandle, index, messageIndex, BufferSize);

    return 0;
}

const char *FinesseGetMessageAuxBufferName(
    finesse_server_handle_t FinesseServerHandle,
    void *Client,
    void *Message)
{
    unsigned index;
    server_internal_connection_state_t *scs = (server_internal_connection_state_t *)FinesseServerHandle;
    unsigned messageIndex;

    for (index = 0; index < SHM_MESSAGE_COUNT; index++) {
        if (scs->client_server_connection_state_table[index] == Client) {
            break;
        }
    }

    assert(index < SHM_MESSAGE_COUNT);

    messageIndex = (unsigned)((((uintptr_t)Message - (uintptr_t)scs->client_server_connection_state_table[index]->client_shm)/SHM_PAGE_SIZE)-1);

    return scs->client_server_connection_state_table[index]->aux_shm_table[messageIndex].AuxShmName;
}

uint64_t FinesseGetActiveClientCount(finesse_server_handle_t FinesseServerHandle)
{
    uint64_t count = 0;
    unsigned index;
    server_internal_connection_state_t *scs = (server_internal_connection_state_t *)FinesseServerHandle;

    assert(NULL != FinesseServerHandle);

    // nothing locks the table, but we don't really care here
    for (index = 0; index < SHM_MESSAGE_COUNT; index++) {
        if (NULL != scs->client_server_connection_state_table[index]) {
            count++;
        }
    }

    return count;

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
