//
// (C) Copyright 2020 Tony Mason
// All Rights Reserved
//
#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif  // _GNU_SOURCE

#include <aio.h>
#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <finesse.h>
#include <mqueue.h>
#include <openssl/sha.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "fcinternal.h"

#define FINESSE_SERVICE_NAME "Finesse-1.0"

const char FinesseSharedMemoryRegionSignature[8] = {'F', 'i', 'n', 'e', 's', 's', 'e'};

const size_t finesse_msg_size             = sizeof(finesse_msg);
const size_t fincomm_message_block_size   = sizeof(fincomm_message_block);
const size_t fincomm_message_header_size  = offsetof(fincomm_message_block, Data);
const size_t fincomm_fuse_request_size    = sizeof(finesse_fuse_request);
const size_t fincomm_native_request_size  = sizeof(finesse_native_request);
const size_t fincomm_fuse_response_size   = sizeof(finesse_fuse_response);
const size_t fincomm_native_response_size = sizeof(finesse_native_response);

//
// This is the common code for the finesse communications package, shared
// between client and server.
//

static void sha256(const char *Data, size_t DataLength, unsigned char *Hash)
{
    SHA256_CTX sha256;

    SHA256_Init(&sha256);
    SHA256_Update(&sha256, Data, DataLength);
    SHA256_Final(Hash, &sha256);
}

//
// Basic design:
//   Initial communications between the Finesse client and the Finesse server is done using
//   a UNIX domain socket; this allows the server to detect when the client has "gone away".
//
//   The primary communications is done via a shared memory region, which is indicated to
//   the server as part of the start-up message.
//
//

int GenerateServerName(const char *MountPath, char *ServerName, size_t ServerNameLength)
{
    int                status;
    unsigned long long hash[SHA256_DIGEST_LENGTH / sizeof(unsigned long long)];
    char               hash_string[1 + (2 * SHA256_DIGEST_LENGTH)];

    status = snprintf(ServerName, ServerNameLength, "%s/%s-%s", FINESSE_SERVICE_PREFIX, FINESSE_SERVICE_NAME, MountPath);
    if ((size_t)status >= ServerNameLength) {
        return EOVERFLOW;
    }

    sha256(ServerName, strlen(ServerName), (unsigned char *)hash);

    _Static_assert((sizeof(hash) % sizeof(unsigned long long)) == 0, "invalid hash size");
    for (unsigned index = 0; index < sizeof(hash) / sizeof(unsigned long long); index++) {
        snprintf(&hash_string[index * sizeof(unsigned long long)], 2 * sizeof(unsigned long long), "%llx", hash[index]);
    }
    hash_string[sizeof(hash_string) - 1] = '\0';

    status = snprintf(ServerName, ServerNameLength, "%s/%s-%s", FINESSE_SERVICE_PREFIX, FINESSE_SERVICE_NAME, hash_string);
    if ((size_t)status >= ServerNameLength) {
        return EOVERFLOW;
    }

    return 0;  // 0 == success
}

int GenerateClientSharedMemoryName(char *SharedMemoryName, size_t SharedMemoryNameLength, uuid_t ClientId)
{
    int  status;
    char uuid_string[40];

    uuid_unparse(ClientId, uuid_string);
    status = snprintf(SharedMemoryName, SharedMemoryNameLength, "%s-%lu", uuid_string, (unsigned long)time(NULL));

    if ((size_t)status >= SharedMemoryNameLength) {
        return EOVERFLOW;
    }
    return 0;  // 0 == success;
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

fincomm_message FinesseGetRequestBuffer(fincomm_shared_memory_region *RequestRegion, FINESSE_MESSAGE_CLASS MessageClass,
                                        int MessageType)
{
    unsigned  index;
    u_int64_t mask = 1;
    u_int64_t bitmap;
    u_int64_t new_bitmap;

    CHECK_SHM_SIGNATURE(RequestRegion);
    assert(NULL != RequestRegion);
    new_bitmap = bitmap = RequestRegion->AllocationBitmap;
    index               = (RequestRegion->LastBufferAllocated + 1) % SHM_MESSAGE_COUNT;
    _Static_assert(64 == SHM_MESSAGE_COUNT, "Check bit mask length");
    new_bitmap |= make_mask64(index);

    // bitmap == new_bitmap - the last request is still in use, so we got unlucky
    // CAS fails - we raced and lost.
    if ((bitmap == new_bitmap) || !__sync_bool_compare_and_swap(&RequestRegion->AllocationBitmap, bitmap, new_bitmap)) {
        // This is the slow path, where we didn't get lucky, so we brute force scan
        for (index = 0; index < SHM_MESSAGE_COUNT; index++, mask = mask << 1) {
            bitmap     = RequestRegion->AllocationBitmap;
            new_bitmap = bitmap | mask;
            if (bitmap == new_bitmap) {
                continue;  // the bit we picked to try is already set
            }
            if (__sync_bool_compare_and_swap(&RequestRegion->AllocationBitmap, bitmap, new_bitmap)) {
                // found our index
                break;
            }
        }
    }

    if (index < SHM_MESSAGE_COUNT) {
        // Note: this is "unsafe" but we're only using it as a hint
        // and thus even if we race, it should work properly.
        // TODO: might want to _use_ this hint (above)
        RequestRegion->LastBufferAllocated = index;
    }

    if (index < SHM_MESSAGE_COUNT) {
        // fprintf(stderr, "%s (%s:%d): thread %d allocated index %u\n", __func__, __FILE__, __LINE__, gettid(), index);
        assert(0 == (RequestRegion->RequestBitmap & make_mask64(index)));
    }

    // TODO: make this blocking?
    // In either case, index indicates the allocated message buffer.  Out of range = alloc failure
    // TODO: should I initialize this region?
    if (index < SHM_MESSAGE_COUNT) {
        // success path
        fincomm_message message = (fincomm_message)&RequestRegion->Messages[index];
        finesse_msg *   fmsg    = (finesse_msg *)message->Data;

        message->MessageType = FINESSE_REQUEST;
        message->Result      = ENOSYS;
        fmsg->Version        = FINESSE_MESSAGE_VERSION;
        fmsg->MessageClass   = MessageClass;

        switch (fmsg->MessageClass) {
            default:
                // Invalid
                assert(0);
                break;
            case FINESSE_FUSE_MESSAGE:
                fmsg->Message.Fuse.Request.Type = (FINESSE_FUSE_REQ_TYPE)MessageType;
                assert((fmsg->Message.Fuse.Request.Type >= FINESSE_FUSE_REQ_BASE_TYPE) &&
                       (fmsg->Message.Fuse.Request.Type < FINESSE_FUSE_REQ_MAX));
                break;
            case FINESSE_NATIVE_MESSAGE:
                fmsg->Message.Native.Request.NativeRequestType = (FINESSE_NATIVE_REQ_TYPE)MessageType;
                assert((fmsg->Message.Native.Request.NativeRequestType >= FINESSE_NATIVE_REQ_BASE_TYPE) &&
                       (fmsg->Message.Native.Request.NativeRequestType < FINESSE_NATIVE_REQ_MAX));
                break;
        }

        FincommCallStatRequestStart(message);

        return message;
    }

    return NULL;
}

u_int64_t FinesseRequestReady(fincomm_shared_memory_region *RequestRegion, fincomm_message Message)
{
    // So the message index can be computed
    unsigned  index      = (unsigned)((((uintptr_t)Message - (uintptr_t)RequestRegion) / SHM_PAGE_SIZE) - 1);
    u_int64_t request_id = 0;

    CHECK_SHM_SIGNATURE(RequestRegion);
    assert(&RequestRegion->Messages[index] == Message);

    if (0 == (RequestRegion->AllocationBitmap & make_mask64(index))) {
        // This is an invalid case
        return 0;
        // assert(0 != (RequestRegion->AllocationBitmap & make_mask64(index)));
        // fprintf(stderr, "%s (%s:%d): thread %d used unallocated index %u\n", __func__, __FILE__, __LINE__, gettid(), index);
    }

    request_id = Message->RequestId = get_request_number(&RequestRegion->RequestId);

    FincommCallStatQueueRequest(Message);

    pthread_mutex_lock(&RequestRegion->RequestMutex);
    if (0 != (RequestRegion->RequestBitmap & make_mask64(index))) {
        // Debug code
        // fprintf(stderr, "%s (%s:%d): thread %d used active index %u\n", __func__, __FILE__, __LINE__, gettid(), index);
        assert(0);
    }
    // assert(0 == (RequestRegion->RequestBitmap & make_mask64(index)));  // this should NOT be set
    RequestRegion->RequestBitmap |= make_mask64(index);
    pthread_cond_signal(&RequestRegion->RequestPending);
    pthread_mutex_unlock(&RequestRegion->RequestMutex);
    // }
    // fprintf(stderr, "%s (%s:%d): thread %d readied index %u\n", __func__, __FILE__, __LINE__, gettid(), index);

    return request_id;
}

void FinesseResponseReady(fincomm_shared_memory_region *RequestRegion, fincomm_message Message, uint32_t Response)
{
    unsigned index = (unsigned)((((uintptr_t)Message - (uintptr_t)RequestRegion) / SHM_PAGE_SIZE) - 1);
    assert(&RequestRegion->Messages[index] == Message);
    (void)Response;  // not used

    CHECK_SHM_SIGNATURE(RequestRegion);

    FincommCallStatQueueResponse(Message);

    pthread_mutex_lock(&RequestRegion->ResponseMutex);
    assert(0 == (RequestRegion->ResponseBitmap & make_mask64(index)));  // this should NOT be set
    RequestRegion->ResponseBitmap |= make_mask64(index);
    pthread_cond_broadcast(&RequestRegion->ResponsePending);
    pthread_mutex_unlock(&RequestRegion->ResponseMutex);
}

int FinesseGetResponse(fincomm_shared_memory_region *RequestRegion, fincomm_message Message, int wait)
{
    unsigned index  = (unsigned)((((uintptr_t)Message - (uintptr_t)RequestRegion) / SHM_PAGE_SIZE) - 1);
    int      status = 0;

    CHECK_SHM_SIGNATURE(RequestRegion);

    assert(&RequestRegion->Messages[index] == Message);
    assert(NULL != RequestRegion);
    assert(NULL != Message);

    pthread_mutex_lock(&RequestRegion->ResponseMutex);
    if (!wait) {
        if (0 != (RequestRegion->ResponseBitmap & make_mask64(index))) {
            status = 1;
            RequestRegion->ResponseBitmap &= ~make_mask64(index);  // clear the response pending bit
        }
    }
    else {
        while (0 == (RequestRegion->ResponseBitmap & make_mask64(index))) {
            pthread_cond_wait(&RequestRegion->ResponsePending, &RequestRegion->ResponseMutex);
        }
        status = 1;
        RequestRegion->ResponseBitmap &= ~make_mask64(index);  // clear the response pending bit
    }
    pthread_mutex_unlock(&RequestRegion->ResponseMutex);
    FincommCallStatDequeueResponse(Message);
    return status;
}

// Blocks until there's something waiting in the target region.
// Returns 0 (success) or ENOTCONN (shutting down)
int FinesseReadyRequestWait(fincomm_shared_memory_region *RequestRegion)
{
    int status = 0;

    CHECK_SHM_SIGNATURE(RequestRegion);

    pthread_mutex_lock(&RequestRegion->RequestMutex);

    while ((0 == RequestRegion->RequestBitmap) && (0 == RequestRegion->ShutdownRequested)) {
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
    unsigned int index = SHM_MESSAGE_COUNT;  // invalid value
    u_int64_t    mask  = 1;
    long int     rnd   = random() % SHM_MESSAGE_COUNT;
    unsigned     i;
    u_int64_t    original_bitmap;
    int          status = EINVAL;

    assert(rnd < SHM_MESSAGE_COUNT);
    pthread_mutex_lock(&RequestRegion->RequestMutex);
    while (SHM_MESSAGE_COUNT == index) {
        // if shutdown requested, we're done
        if (RequestRegion->ShutdownRequested) {
            index  = SHM_MESSAGE_COUNT;  // no allocation in the shutdown path
            status = ENOTCONN;
            break;
        }

        if (0 == RequestRegion->RequestBitmap) {
            status = ENOENT;
            break;
        }

        // start from the random value and start looking
        mask            = make_mask64(rnd);
        original_bitmap = RequestRegion->RequestBitmap;
        for (i = rnd; i < SHM_MESSAGE_COUNT; i++) {
            if (RequestRegion->RequestBitmap & mask) {
                // found one!
                RequestRegion->RequestBitmap &= ~mask;  // clear the bit
                index = i;                              // note which one we found
                break;
            }
            mask = mask << 1;
        }
        if (i < SHM_MESSAGE_COUNT) {
            index  = i;
            status = 0;
            break;  // we already found one
        }

        // now check from 0 to the random value
        mask = 1;
        for (i = 0; i < rnd; i++) {
            if (RequestRegion->RequestBitmap & mask) {
                // found one!
                RequestRegion->RequestBitmap &= ~mask;  // clear the bit
                index = i;                              // note which one we found
                break;
            }
            mask = mask << 1;
        }

        // If we found one, note which one and break out
        if (i < rnd) {
            index  = i;
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
        assert(index < SHM_MESSAGE_COUNT);
        assert(0 != RequestRegion->Messages[index].RequestId);
        *message = &RequestRegion->Messages[index];
        FincommCallStatDequeueRequest(*message);
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
    pthread_condattr_t  cattr;
    int                 status;

    assert(NULL != Fsmr);
    assert(sizeof(Fsmr->Signature) == sizeof(FinesseSharedMemoryRegionSignature));
    memcpy(Fsmr->Signature, FinesseSharedMemoryRegionSignature, sizeof(FinesseSharedMemoryRegionSignature));
    uuid_generate(Fsmr->ClientId);
    uuid_generate(Fsmr->ServerId);
    Fsmr->RequestBitmap  = 0;
    Fsmr->ResponseBitmap = 0;
    Fsmr->RequestWaiters = 0;
    memset(Fsmr->UnusedRegion, 0, sizeof(Fsmr->UnusedRegion));
    Fsmr->AllocationBitmap    = 0;
    Fsmr->RequestId           = (u_int64_t)(-10);
    Fsmr->ShutdownRequested   = 0;
    Fsmr->LastBufferAllocated = SHM_MESSAGE_COUNT - 1;  // so we start at 0

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
    int      status;
    unsigned retry = 0;

    CHECK_SHM_SIGNATURE(Fsmr);

    assert(NULL != Fsmr);
    assert(0 == Fsmr->ShutdownRequested);  // don't call twice!
    assert(0 == Fsmr->AllocationBitmap);   // shouldn't have any outstanding buffer allocations!

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
