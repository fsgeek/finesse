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
#include "fincomm.h"

#define FINESSE_SERVICE_NAME "Finesse-1.0"


#define make_mask64(index) (((u_int64_t)1)<<index)

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
    // static u_int64_t RequestNumber = (uint64_t)(-10); // start just below zero to ensure we wrap properly
    // static u_int64_t BufferAllocationBitmap;

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
    return (index < SHM_MESSAGE_COUNT) ? &RequestRegion->Messages[index] : NULL;
}

u_int64_t FinesseRequestReady(fincomm_shared_memory_region *RequestRegion, fincomm_message Message)
{
    // So the message index can be computed
    unsigned index = (unsigned)((((uintptr_t)Message - (uintptr_t)RequestRegion)/SHM_PAGE_SIZE)-1);
    u_int64_t request_id = 0;
    assert(&RequestRegion->Messages[index] == Message);

    if (0 != (RequestRegion->AllocationBitmap & make_mask64(index))) {
        request_id = Message->RequestId = get_request_number(&RequestRegion->RequestId);
        Message->Response = 0;

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

    pthread_mutex_lock(&RequestRegion->ResponseMutex);
    assert(0 == (RequestRegion->ResponseBitmap & make_mask64(index))); // this should NOT be set
    RequestRegion->ResponseBitmap |= make_mask64(index);
    Message->Response = Response;
    pthread_cond_broadcast(&RequestRegion->ResponsePending);
    pthread_mutex_unlock(&RequestRegion->ResponseMutex);
}

int FinesseGetResponse(fincomm_shared_memory_region *RequestRegion, fincomm_message Message, int wait)
{
    unsigned index = (unsigned)((((uintptr_t)Message - (uintptr_t)RequestRegion)/SHM_PAGE_SIZE)-1);
    int status = 0;

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

fincomm_message FinesseGetReadyRequest(fincomm_shared_memory_region *RequestRegion)
{
    unsigned int index = SHM_MESSAGE_COUNT; // invalid value
    u_int64_t mask = 1;
    long int rnd = random() % SHM_MESSAGE_COUNT;
    unsigned i;
    u_int64_t original_bitmap;

    assert(rnd < SHM_MESSAGE_COUNT);
    pthread_mutex_lock(&RequestRegion->RequestMutex);
    while (SHM_MESSAGE_COUNT == index) {

        // wait for notification that something is pending (or shutdown)
        while ((0 == RequestRegion->RequestBitmap) && (0 == RequestRegion->ShutdownRequested)){
            RequestRegion->RequestWaiters++;
            pthread_cond_wait(&RequestRegion->RequestPending, &RequestRegion->RequestMutex);
            RequestRegion->RequestWaiters--;
        }

        // if shutdown requested, we're done
        if (RequestRegion->ShutdownRequested) {
            index = SHM_MESSAGE_COUNT; // no allocation in the shutdown path
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
            break;
        }

        // otherwise, we could have raced, so we loop again
    }
    if (index < SHM_MESSAGE_COUNT) {
        // sanity: make sure the bit we're clearing was set
        assert(original_bitmap & make_mask64(index));
    }
    pthread_mutex_unlock(&RequestRegion->RequestMutex);
    if (index < SHM_MESSAGE_COUNT) {
        assert(0 != RequestRegion->Messages[index].RequestId);
        return &RequestRegion->Messages[index];
    }
    else {
        return NULL; // shutdown path.
    }
}

void FinesseReleaseRequestBuffer(fincomm_shared_memory_region *RequestRegion, fincomm_message Message)
{
    unsigned index = (unsigned)((((uintptr_t)Message - (uintptr_t)RequestRegion)/SHM_PAGE_SIZE)-1);
    u_int64_t bitmap; // = AllocationBitmap;
    u_int64_t new_bitmap; 

    assert(NULL != RequestRegion);
    assert(index < SHM_MESSAGE_COUNT);
    assert(NULL != Message);

    Message->RequestId = 0; // invalid

    bitmap = RequestRegion->AllocationBitmap;
    new_bitmap = bitmap & ~make_mask64(index);
    assert(bitmap != new_bitmap); // freeing an unallocated message

    assert(&RequestRegion->Messages[index] == Message);

    while (!__sync_bool_compare_and_swap(&RequestRegion->AllocationBitmap, bitmap, new_bitmap)) {
        bitmap = RequestRegion->AllocationBitmap;
        new_bitmap = (bitmap & ~make_mask64(index));
    }
}

int FinesseInitializeMemoryRegion(fincomm_shared_memory_region *Fsmr)
{
    pthread_mutexattr_t mattr;
    pthread_condattr_t cattr;
    int status;

    assert(NULL != Fsmr);
    uuid_generate(Fsmr->ClientId);
    uuid_generate(Fsmr->ServerId);
    Fsmr->RequestBitmap = 0;
    Fsmr->ResponseBitmap = 0;
    Fsmr->RequestWaiters = 0;
    Fsmr->secondary_shm_path[0] = '\0';
    memset(Fsmr->Data, 0, sizeof(Fsmr->Data));
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

    assert(NULL != Fsmr);
    assert(0 == Fsmr->ShutdownRequested); // don't call twice!

    Fsmr->ShutdownRequested = 1;
    pthread_mutex_lock(&Fsmr->RequestMutex);
    while (Fsmr->RequestWaiters > 0) {
        pthread_mutex_unlock(&Fsmr->RequestMutex);
        pthread_cond_broadcast(&Fsmr->RequestPending);
        sleep(1);
        pthread_mutex_lock(&Fsmr->RequestMutex);
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

