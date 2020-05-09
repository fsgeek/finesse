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
    u_char      Data[4096-16];
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
    pthread_mutex_t RequestMutex;
    pthread_cond_t  RequestPending;
    u_char          align0[128-((2 * sizeof(uuid_t)) + sizeof(u_int64_t) + sizeof(pthread_mutex_t) + sizeof(pthread_cond_t))];
    u_int64_t       ResponseBitmap;
    pthread_mutex_t ResponseMutex;
    pthread_cond_t  ResponsePending;
    u_char          align1[128-(sizeof(u_int64_t) + sizeof(pthread_mutex_t) + sizeof(pthread_cond_t))];
    char            secondary_shm_path[MAX_SHM_PATH_NAME];
    u_char          Data[4096-(3*128)];
    fincomm_message_block   Messages[SHM_MESSAGE_COUNT];
} fincomm_shared_memory_region;

_Static_assert(0 == sizeof(fincomm_shared_memory_region) % OPTIMAL_ALIGNMENT_SIZE, "Alignment wrong");
_Static_assert(0 == offsetof(fincomm_shared_memory_region, ResponseBitmap) % OPTIMAL_ALIGNMENT_SIZE, "Alignment wrong");
_Static_assert(0 == sizeof(fincomm_shared_memory_region) % SHM_PAGE_SIZE, "Length Wrong");

int GenerateServerName(char *ServerName, size_t ServerNameLength);
int GenerateClientSharedMemoryName(char *SharedMemoryName, size_t SharedMemoryNameLength, uuid_t ClientId);

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
fincomm_message FinesseGetReadyRequest(fincomm_shared_memory_region *RequestRegion);
void FinesseReleaseRequestBuffer(fincomm_shared_memory_region *RequestRegion, fincomm_message Message);


#endif // __FINESSE_FINCOMM_H__
