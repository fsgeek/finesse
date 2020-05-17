//
// (C) Copyright 2020 Tony Mason
// All Rights Reserved
//
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
#include <finesse.h>
#include "fcinternal.h"

#define FINESSE_SERVICE_NAME "Finesse-1.0"

const char FinesseSharedMemoryRegionSignature[8] = {'F','i','n','e','s','s','e'};

//
// This is the common code for the finesse communications package, shared
// between client and server.
//

//
// Basic design:
//   Initial communications between the Finesse client and the Finesse server is done using
//   a UNIX domain socket; this allows the server to detect when the client has "gone away".
//
//   The primary communications is done via a shared memory region, which is indicated to
//   the server as part of the start-up message.
//
//   

int GenerateServerName(char *ServerName, size_t ServerNameLength)
{
    int status;

    status = snprintf(ServerName, ServerNameLength, "%s/%s", FINESSE_SERVICE_PREFIX, FINESSE_SERVICE_NAME);
    if (status >= ServerNameLength) {
        return EOVERFLOW;
    }
    return 0; // 0 == success
}

int GenerateClientSharedMemoryName(char *SharedMemoryName, size_t SharedMemoryNameLength, uuid_t ClientId)
{
    int status;
    char uuid_string[40];

    uuid_unparse(ClientId, uuid_string);
    status = snprintf(SharedMemoryName, SharedMemoryNameLength, "%s-%lu", uuid_string, (unsigned long)time(NULL));

    if (status >= SharedMemoryNameLength) {
        return EOVERFLOW;
    }
    return 0; // 0 == success;
}


static u_int64_t get_request_number(u_int64_t *RequestNumber)
{
    u_int64_t request_number = 0;

    // request_number 0 is not valid
    while (0 == request_number) {
        request_number = __sync_fetch_and_add(RequestNumber, 1);
    }
    assert(0 != request_number);
    return request_number;
}

fincomm_message FinesseGetRequestBuffer(fincomm_shared_memory_region *RequestRegion)
{
    unsigned index;
    u_int64_t mask = 1;
    u_int64_t bitmap;
    u_int64_t new_bitmap;

    CHECK_SHM_SIGNATURE(RequestRegion);
    assert(NULL != RequestRegion);
    new_bitmap = bitmap = RequestRegion->AllocationBitmap;
    index = (RequestRegion->LastBufferAllocated + 1) % SHM_MESSAGE_COUNT;
    _Static_assert(64 == SHM_MESSAGE_COUNT, "Check bit mask length");
    new_bitmap |= make_mask64(index);

    if (!__sync_bool_compare_and_swap(&RequestRegion->AllocationBitmap, bitmap, new_bitmap)) {
        // This is the slow path, where we didn't get lucky, so we brute force scan
        for (index = 0; index < SHM_MESSAGE_COUNT; index++, mask = mask << 1) {
            bitmap = RequestRegion->AllocationBitmap;
            new_bitmap = bitmap | mask;
            if (__sync_bool_compare_and_swap(&RequestRegion->AllocationBitmap, bitmap, new_bitmap)) {
                // found our index
                break;
            }
        }
    }

    if (index < SHM_MESSAGE_COUNT) {
        // Note: this is "unsafe" but we're only using it as a hint
        // and thus even if we race, it should work properly.
        RequestRegion->LastBufferAllocated = index;
    }

    // TODO: make this blocking?
    // In either case, index indicates the allocated message buffer.  Out of range = alloc failure
    // TODO: should I initialize this region?
    return (index < SHM_MESSAGE_COUNT) ? (fincomm_message)&RequestRegion->Messages[index] : NULL;
}

u_int64_t FinesseRequestReady(fincomm_shared_memory_region *RequestRegion, fincomm_message Message)
{
    // So the message index can be computed
    unsigned index = (unsigned)((((uintptr_t)Message - (uintptr_t)RequestRegion)/SHM_PAGE_SIZE)-1);
    u_int64_t request_id = 0;

    CHECK_SHM_SIGNATURE(RequestRegion);
    assert(&RequestRegion->Messages[index] == Message);


    if (0 != (RequestRegion->AllocationBitmap & make_mask64(index))) {
        request_id = Message->RequestId = get_request_number(&RequestRegion->RequestId);

        pthread_mutex_lock(&RequestRegion->RequestMutex);
        assert(0 == (RequestRegion->RequestBitmap & make_mask64(index))); // this should NOT be set
        RequestRegion->RequestBitmap |= make_mask64(index);
        pthread_cond_signal(&RequestRegion->RequestPending);
        pthread_mutex_unlock(&RequestRegion->RequestMutex);
    }

    return request_id;
}

void FinesseResponseReady(fincomm_shared_memory_region *RequestRegion, fincomm_message Message, uint32_t Response)
{
    unsigned index = (unsigned)((((uintptr_t)Message - (uintptr_t)RequestRegion)/SHM_PAGE_SIZE)-1);
    assert(&RequestRegion->Messages[index] == Message);
    (void) Response; // not used

    CHECK_SHM_SIGNATURE(RequestRegion);

    pthread_mutex_lock(&RequestRegion->ResponseMutex);
    assert(0 == (RequestRegion->ResponseBitmap & make_mask64(index))); // this should NOT be set
    RequestRegion->ResponseBitmap |= make_mask64(index);
    pthread_cond_broadcast(&RequestRegion->ResponsePending);
    pthread_mutex_unlock(&RequestRegion->ResponseMutex);
}

int FinesseGetResponse(fincomm_shared_memory_region *RequestRegion, fincomm_message Message, int wait)
{
    unsigned index = (unsigned)((((uintptr_t)Message - (uintptr_t)RequestRegion)/SHM_PAGE_SIZE)-1);
    int status = 0;

    CHECK_SHM_SIGNATURE(RequestRegion);

    assert(&RequestRegion->Messages[index] == Message);
    assert(NULL != RequestRegion);
    assert(NULL != Message);

    pthread_mutex_lock(&RequestRegion->ResponseMutex);
    if (!wait) {
        if (0 != (RequestRegion->ResponseBitmap & make_mask64(index))) {
            status = 1;
            RequestRegion->ResponseBitmap &= ~make_mask64(index); // clear the response pending bit
        }

    }
    else {
        while (0 == (RequestRegion->ResponseBitmap & make_mask64(index))) {
            pthread_cond_wait(&RequestRegion->ResponsePending, &RequestRegion->ResponseMutex);\
        }
        status = 1;
        RequestRegion->ResponseBitmap &= ~make_mask64(index); // clear the response pending bit
    }
    pthread_mutex_unlock(&RequestRegion->ResponseMutex);
    return status;
}

// Blocks until there's something waiting in the target region.
// Returns 0 (success) or ENOTCONN (shutting down)
int FinesseReadyRequestWait(fincomm_shared_memory_region *RequestRegion)
{
    int status = 0;

    CHECK_SHM_SIGNATURE(RequestRegion);

    pthread_mutex_lock(&RequestRegion->RequestMutex);

    while ((0 == RequestRegion->RequestBitmap) && (0 == RequestRegion->ShutdownRequested)){
        RequestRegion->RequestWaiters++;
        pthread_cond_wait(&RequestRegion->RequestPending, &RequestRegion->RequestMutex);
        RequestRegion->RequestWaiters--;
    }
    pthread_mutex_unlock(&RequestRegion->RequestMutex);

    if (RequestRegion->ShutdownRequested) {
        status = ENOTCONN;
    }

    return status;
}

// Returns a ready request; if there isn't one, it returns ENOENT.  ENOTCONN returned for shutdown.
// DOES NOT BLOCK.
int FinesseGetReadyRequest(fincomm_shared_memory_region *RequestRegion, fincomm_message *message)
{
    unsigned int index = SHM_MESSAGE_COUNT; // invalid value
    u_int64_t mask = 1;
    long int rnd = random() % SHM_MESSAGE_COUNT;
    unsigned i;
    u_int64_t original_bitmap;
    int status = EINVAL;

    assert(rnd < SHM_MESSAGE_COUNT);
    pthread_mutex_lock(&RequestRegion->RequestMutex);
    while (SHM_MESSAGE_COUNT == index) {

        // if shutdown requested, we're done
        if (RequestRegion->ShutdownRequested) {
            index = SHM_MESSAGE_COUNT; // no allocation in the shutdown path
            status = ENOTCONN;
            break;
        }

        if (0 == RequestRegion->RequestBitmap) {
            status = ENOENT;
            break;
        }

        // start from the random value and start looking
        mask = make_mask64(rnd);
        original_bitmap = RequestRegion->RequestBitmap;
        for (i = rnd; i < SHM_MESSAGE_COUNT; i++) {
            if (RequestRegion->RequestBitmap & mask) {
                // found one!
                RequestRegion->RequestBitmap &= ~mask; // clear the bit
                index = i; // note which one we found
                break;
            }
            mask = mask << 1;
        }
        if (i < SHM_MESSAGE_COUNT) {
            index = i;
            status = 0;
            break; // we already found one
        }

        // now check from 0 to the random value
        mask = 1;
        for (i = 0; i < rnd; i++) {
            if (RequestRegion->RequestBitmap & mask) {
                // found one!
                RequestRegion->RequestBitmap &= ~mask; // clear the bit
                index = i; // note which one we found
                break;
            }
            mask = mask << 1;
        }

        // If we found one, note which one and break out
        if (i < rnd) {
            index = i;
            status = 0;
            break;
        }

        // otherwise, we could have raced, so we loop again
    }
    if (index < SHM_MESSAGE_COUNT) {
        // sanity: make sure the bit we're clearing was set
        assert(original_bitmap & make_mask64(index));
    }
    pthread_mutex_unlock(&RequestRegion->RequestMutex);
    
    if (0 == status) {
        assert (index < SHM_MESSAGE_COUNT);        
        assert(0 != RequestRegion->Messages[index].RequestId);
        *message = &RequestRegion->Messages[index];
    }
    else {
        assert(SHM_MESSAGE_COUNT == index);
        *message = NULL;
    }
    return status;
}

int FinesseInitializeMemoryRegion(fincomm_shared_memory_region *Fsmr)
{
    pthread_mutexattr_t mattr;
    pthread_condattr_t cattr;
    int status;

    assert(NULL != Fsmr);
    assert(sizeof(Fsmr->Signature) == sizeof(FinesseSharedMemoryRegionSignature));
    memcpy(Fsmr->Signature, FinesseSharedMemoryRegionSignature, sizeof(FinesseSharedMemoryRegionSignature));
    uuid_generate(Fsmr->ClientId);
    uuid_generate(Fsmr->ServerId);
    Fsmr->RequestBitmap = 0;
    Fsmr->ResponseBitmap = 0;
    Fsmr->RequestWaiters = 0;
    memset(Fsmr->UnusedRegion, 0, sizeof(Fsmr->UnusedRegion));
    Fsmr->AllocationBitmap = 0;
    Fsmr->RequestId = (u_int64_t)(-10);
    Fsmr->ShutdownRequested = 0;
    Fsmr->LastBufferAllocated = SHM_MESSAGE_COUNT - 1; // so we start at 0

    status = pthread_mutexattr_init(&mattr);
    assert(0 == status);
    status = pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
    assert(0 == status);
    status = pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_ERRORCHECK_NP);
    assert(0 == status);
    status = pthread_mutex_init(&Fsmr->RequestMutex, &mattr);
    assert(0 == status);
    status = pthread_mutex_init(&Fsmr->ResponseMutex, &mattr);
    assert(0 == status);
    status = pthread_mutexattr_destroy(&mattr);
    assert(0 == status);

    status = pthread_condattr_init(&cattr);
    assert(0 == status);
    status = pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);
    assert(0 == status);
    status = pthread_cond_init(&Fsmr->RequestPending, &cattr);
    assert(0 == status);
    status = pthread_cond_init(&Fsmr->ResponsePending, &cattr);
    assert(0 == status);
    status = pthread_condattr_destroy(&cattr);
    assert(0 == status);

    return status;
}

int FinesseDestroyMemoryRegion(fincomm_shared_memory_region *Fsmr)
{
    int status;
    unsigned retry = 0;

    CHECK_SHM_SIGNATURE(Fsmr);

    assert(NULL != Fsmr);
    assert(0 == Fsmr->ShutdownRequested); // don't call twice!
    assert(0 == Fsmr->AllocationBitmap); // shouldn't have any outstanding buffer allocations!

    Fsmr->ShutdownRequested = 1;
    pthread_mutex_lock(&Fsmr->RequestMutex);
    while (Fsmr->RequestWaiters > 0) {
        pthread_mutex_unlock(&Fsmr->RequestMutex);
        pthread_cond_broadcast(&Fsmr->RequestPending);
        sleep(1);
        pthread_mutex_lock(&Fsmr->RequestMutex);
        if (retry++ > 5) {
            // Server side can cause this to hang.
            // Should find some way to fix this.
            // TODO: deal with this since it could
            // crash the server.
            break;
        }
    }
    pthread_mutex_unlock(&Fsmr->RequestMutex);


    status = pthread_mutex_destroy(&Fsmr->RequestMutex);
    assert(0 == status);

    status = pthread_mutex_destroy(&Fsmr->ResponseMutex);
    assert(0 == status);

    status = pthread_cond_destroy(&Fsmr->RequestPending);
    assert(0 == status);

    status = pthread_cond_destroy(&Fsmr->ResponsePending);
    assert(0 == status);

    return status;
}


