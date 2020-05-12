//
// (C) Copyright 2020 Tony Mason
// All Rights Reserved
//
#define _GNU_SOURCE

#pragma once

#if !defined(__FINESSE_FINCOMM_H__)
#define __FINESSE_FINCOMM_H__ (1)

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
#include <uuid/uuid.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <finesse.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <dirent.h>
#include <sys/mman.h>
#include <finesse_fuse_msg.h>


#define FINESSE_SERVICE_PREFIX "/tmp/finesse"


#define OPTIMAL_ALIGNMENT_SIZE (64) // this should be whatever the best choice is for laying out cache line optimal data structures
#define MAX_SHM_PATH_NAME (128)     // secondary shared memory regions must fit within a buffer of this size (including a null terminator)
#define SHM_MESSAGE_COUNT (64)         // this is the maximum number of parallel/simultaneous messages per client.
#define SHM_PAGE_SIZE (4096)        // this should be the page size of the underlying machine.

//
// The registration structure is how the client connects to the server
//
typedef struct {
    uuid_t      ClientId;
    u_int32_t   ClientSharedMemPathNameLength;
    char        ClientSharedMemPathName[MAX_SHM_PATH_NAME];
} fincomm_registration_info;

typedef struct {
    uuid_t      ServerId;
    size_t      ClientSharedMemSize;
    u_int32_t   Result;
} fincomm_registration_confirmation;

//
// Each shared memory region consists of a set of communications blocks
//
typedef struct {
    u_int64_t   RequestId;
    u_int32_t   RequestType;
    u_int32_t   Response;
    u_int8_t    Data[4096-16];
} fincomm_message_block;

typedef fincomm_message_block *fincomm_message;

// ensure we have proper cache line alignment
_Static_assert(0 == sizeof(fincomm_message_block) % OPTIMAL_ALIGNMENT_SIZE, "Alignment wrong");
_Static_assert(SHM_PAGE_SIZE == sizeof(fincomm_message_block), "Alignment wrong");


//
// The shared memory region has a header, followed by (page aligned)
// message blocks
//
typedef struct {
    uuid_t          ClientId;
    uuid_t          ServerId;
    u_int64_t       RequestBitmap;
    u_int64_t       RequestWaiters;
    pthread_mutex_t RequestMutex;
    pthread_cond_t  RequestPending;
    u_int8_t        align0[192-((2 * sizeof(uuid_t)) + (2*sizeof(u_int64_t)) + sizeof(pthread_mutex_t) + sizeof(pthread_cond_t))];
    u_int64_t       ResponseBitmap;
    pthread_mutex_t ResponseMutex;
    pthread_cond_t  ResponsePending;
    u_int8_t        align1[128-(sizeof(u_int64_t) + sizeof(pthread_mutex_t) + sizeof(pthread_cond_t))];
    char            secondary_shm_path[MAX_SHM_PATH_NAME];
    unsigned        LastBufferAllocated; // allocation hint
    u_int64_t       AllocationBitmap;
    u_int64_t       RequestId;
    u_int64_t       ShutdownRequested;
    u_int8_t        align2[64-(4 * sizeof(u_int64_t))];
    u_int8_t        Data[4096-(8*64)];
    fincomm_message_block   Messages[SHM_MESSAGE_COUNT];
} fincomm_shared_memory_region;

_Static_assert(0 == sizeof(fincomm_shared_memory_region) % OPTIMAL_ALIGNMENT_SIZE, "Alignment wrong");
_Static_assert(0 == offsetof(fincomm_shared_memory_region, ResponseBitmap) % OPTIMAL_ALIGNMENT_SIZE, "Alignment wrong");
_Static_assert(0 == offsetof(fincomm_shared_memory_region, secondary_shm_path) % OPTIMAL_ALIGNMENT_SIZE, "Alignment wrong");
_Static_assert(0 == offsetof(fincomm_shared_memory_region, LastBufferAllocated) % OPTIMAL_ALIGNMENT_SIZE, "Alignment wrong");
_Static_assert(0 == offsetof(fincomm_shared_memory_region, Data) % OPTIMAL_ALIGNMENT_SIZE, "Alignment wrong");
_Static_assert(0 == offsetof(fincomm_shared_memory_region, Messages) % SHM_PAGE_SIZE, "Alignment wrong");
_Static_assert(0 == sizeof(fincomm_shared_memory_region) % SHM_PAGE_SIZE, "Length Wrong");

int GenerateServerName(char *ServerName, size_t ServerNameLength);
int GenerateClientSharedMemoryName(char *SharedMemoryName, size_t SharedMemoryNameLength, uuid_t ClientId);


typedef struct _client_connection_state {
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
} client_connection_state_t;

typedef struct server_connection_state {
    fincomm_registration_info       reg_info;
    int                             client_connection;
    int                             client_shm_fd;
    size_t                          client_shm_size;
    void *                          client_shm;
    int                             aux_shm_fd;
    int                             aux_shm_size;
    void *                          aux_shm;
    pthread_t                       monitor_thread;
    uint8_t                         monitor_thread_active;
    char                            aux_shm_path[MAX_SHM_PATH_NAME];
} server_connection_state_t;


//
// This is the shared memory protocol:
//   (1) client allocates a request region (FinesseGetRequestBuffer)
//   (2) client sets up the request (message->Data)
//   (3) client asks for server notification (FinesseRequestReady)
//   (4) server retrieves message (FinesseGetReadyRequest)
//   (5) server constructs response in-place
//   (6) server notifies client (FinesseResponseReady)
//   (7) client can poll or block for response (FinesseGetResponse)
//   (8) client frees the request region (FinesseReleaseRequestBuffer)
//
// The goal is, as much as possible, to avoid synchronization. While I'm using condition variables
// now, I was thinking it might be better to use the IPC channel for sending messages, but
// I'm not going to address that today.
//
fincomm_message FinesseGetRequestBuffer(fincomm_shared_memory_region *RequestRegion);
u_int64_t FinesseRequestReady(fincomm_shared_memory_region *RequestRegion, fincomm_message Message);
void FinesseResponseReady(fincomm_shared_memory_region *RequestRegion, fincomm_message Message, uint32_t Response);
int FinesseGetResponse(fincomm_shared_memory_region *RequestRegion, fincomm_message Message, int wait);
int FinesseGetReadyRequest(fincomm_shared_memory_region *RequestRegion, fincomm_message *message);
int FinesseReadyRequestWait(fincomm_shared_memory_region *RequestRegion);
void FinesseReleaseRequestBuffer(fincomm_shared_memory_region *RequestRegion, fincomm_message Message);
int FinesseInitializeMemoryRegion(fincomm_shared_memory_region *Fsmr);
int FinesseDestroyMemoryRegion(fincomm_shared_memory_region *Fsmr);

#endif // __FINESSE_FINCOMM_H__

