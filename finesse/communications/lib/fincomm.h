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
#define FINESSE_SERVICE_NAME "Finesse-1.0"

#define OPTIMAL_ALIGNMENT_SIZE (64) // this should be whatever the best choice is for laying out cache line optimal data structures
#define MAX_SHM_PATH_NAME (128)     // secondary shared memory regions must fit within a buffer of this size (including a null terminator)
#define SHM_PAGE_COUNT (64)         // this is the maximum number of parallel/simultaneous messages per client.
#define SHM_PAGE_SIZE (4096)        // this should be the page size of the underlying machine.

//
// The registration structure is how the client connects to the server
//
typedef struct {
    uuid_t     ClientId;
    u_int32_t  ClientSharedMemPathNameLength;
    char     ClientSharedMemPathName[MAX_SHM_PATH_NAME];
} fincomm_registration_info;

//
// Each shared memory region consists of a set of communications blocks
//

typedef struct {
    uuid_t     RequestId;
    u_int64_t  RequestReady;
    u_char     align0[40];
    u_int64_t  ResponseReady;
    u_int64_t  ResponseStatus;
    char       secondary_shm_path[MAX_SHM_PATH_NAME];
    u_char     Data[3888];
} fincomm_shared_memory_block;

_Static_assert(0 == sizeof(fincomm_shared_memory_block) % OPTIMAL_ALIGNMENT_SIZE, "Alignment wrong");
_Static_assert(0 == offsetof(fincomm_shared_memory_block, ResponseReady) % OPTIMAL_ALIGNMENT_SIZE, "Alignment wrong");
_Static_assert(SHM_PAGE_SIZE == sizeof(fincomm_shared_memory_block), "Alignment wrong");


typedef struct {
    uuid_t    ClientId;
    u_int64_t RequestBitmap;
    u_char    align0[40];
    u_int64_t ResponseBitmap;
    u_char    align1[4024];
} fincomm_shared_memory_header;

// ensure we have proper cache line alignment
_Static_assert(0 == sizeof(fincomm_shared_memory_header) % OPTIMAL_ALIGNMENT_SIZE, "Alignment wrong");
_Static_assert(0 == offsetof(fincomm_shared_memory_header, ResponseBitmap) % OPTIMAL_ALIGNMENT_SIZE, "Alignment wrong");
_Static_assert(SHM_PAGE_SIZE == sizeof(fincomm_shared_memory_header), "Alignment wrong");

#endif // __FINESSE_FINCOMM_H__
