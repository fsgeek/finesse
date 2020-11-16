#include <assert.h>
#include <malloc.h>
#include <memory.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "fcinternal.h"

/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

//
// This module is used for keeping track of communications call overhead.
// There are four timestamps:
//   * Request Send Time - this is written when the client readies the message
//   * Request Receive Time - this is written when the server starts processing the message.
//   * Response Send Time - this is written when the server sends the message
//   * Response Receive Time - this is written when the client receives the response
//
// The communications package can then update this data as the messages transition between
// states.  When the message is freed (by the original client), it can compute the amount
// of time spent in each of these stages, by call type (and possibly success/failure status)
// to better understand where time is being spent inside the communications layer.
//

#if 0
struct timespec RequestStartTime;
struct timespec RequestQueueTime;
struct timespec RequestDequeueTime;
struct timespec ResponseQueueTime;
struct timespec ResponseReceiptTime;
struct timespec RequestCompeltionTime;
#endif  // 0

typedef struct _fincomm_call_time {
    struct timespec RequestCreateTime;       // queue - start
    struct timespec RequestQueueDelay;       // dequeue - queue
    struct timespec ResponseProcessingTime;  // response queue - dequeue
    struct timespec ResponseQueueDelay;      // response receipt - response queue
    struct timespec RequestTotalTime;        // finish - start
} fincomm_call_time_t;

typedef struct _fincomm_api_call_statistics {
    uint64_t            Calls;
    uint64_t            Success;
    uint64_t            Failure;
    fincomm_call_time_t SuccessTimings;
    fincomm_call_time_t FailureTimings;
    pthread_mutex_t     Lock;
} fincomm_api_call_statistics_t;

struct _fincomm_call_data {
    fincomm_api_call_statistics_t Fuse[FINESSE_FUSE_REQ_MAX - FINESSE_FUSE_REQ_LOOKUP];
    fincomm_api_call_statistics_t Native[FINESSE_NATIVE_REQ_MAX - FINESSE_NATIVE_REQ_TEST];
} fincomm_call_data;

static int      commstat_initialized = 0;
pthread_mutex_t commstat_init_lock   = PTHREAD_MUTEX_INITIALIZER;

// TODO: move to a header file
void FincommRecordStats(fincomm_message Response);
void FincommCallStatRequestStart(fincomm_message Message);
void FincommCallStatQueueRequest(fincomm_message Message);
void FincommCallStatDequeueRequest(fincomm_message Message);
void FincommCallStatQueueResponse(fincomm_message Message);
void FincommCallStatDequeueResponse(fincomm_message Message);
void FincommCallStatCompleteRequest(fincomm_message Message);

// Note: this code is copied from callstats.h - probably should extract into a common header

#if 0   // apparently, this isn't currently being used
static inline void timespec_diff(struct timespec *begin, struct timespec *end, struct timespec *diff)
{
    struct timespec result = {.tv_sec = 0, .tv_nsec = 0};
    assert((end->tv_sec > begin->tv_sec) || ((end->tv_sec == begin->tv_sec) && end->tv_nsec >= begin->tv_nsec));
    result.tv_sec = end->tv_sec - begin->tv_sec;
    if (end->tv_nsec < begin->tv_nsec) {
        result.tv_sec--;
        result.tv_nsec = (long)1000000000 + end->tv_nsec - begin->tv_nsec;
    }
    *diff = result;
}
#endif  // 0

static inline void timespec_add(struct timespec *one, struct timespec *two, struct timespec *result)
{
    result->tv_sec  = one->tv_sec + two->tv_sec;
    result->tv_nsec = one->tv_nsec + two->tv_nsec;
    while ((long)1000000000 <= result->tv_nsec) {
        result->tv_sec++;
        result->tv_nsec -= (long)1000000000;
    }
}

static void commstat_init(void)
{
    while (0 == commstat_initialized) {
        pthread_mutex_lock(&commstat_init_lock);
        if (0 == commstat_initialized) {
            commstat_initialized = 1;

            // fincomm_api_call_statistics_t Fuse[FINESSE_FUSE_REQ_MAX - FINESSE_FUSE_REQ_LOOKUP];
            // fincomm_api_call_statistics_t Native[FINESSE_NATIVE_REQ_MAX - FINESSE_NATIVE_REQ_TEST];
            for (unsigned index = 0; index < sizeof(fincomm_call_data.Fuse) / sizeof(fincomm_api_call_statistics_t); index++) {
                pthread_mutex_init(&fincomm_call_data.Fuse[index].Lock, NULL);
            }
            for (unsigned index = 0; index < sizeof(fincomm_call_data.Native) / sizeof(fincomm_api_call_statistics_t); index++) {
                pthread_mutex_init(&fincomm_call_data.Native[index].Lock, NULL);
            }
        }
        pthread_mutex_unlock(&commstat_init_lock);
    }
}

void FincommRecordStats(fincomm_message Response)
{
    fincomm_api_call_statistics_t *callstats = NULL;
    unsigned                       index     = ~0;  // bogus value
    finesse_msg *                  message;

    if (0 == commstat_initialized) {
        commstat_init();
    }

    assert(0 != commstat_initialized);
    assert(NULL != Response);

    message = (finesse_msg *)Response->Data;
    if (FINESSE_FUSE_MESSAGE == message->Stats.RequestClass) {
        index = (unsigned)message->Stats.RequestType.Fuse;
        assert((index >= FINESSE_FUSE_REQ_BASE_TYPE) && (index < FINESSE_FUSE_REQ_MAX));
        index -= FINESSE_FUSE_REQ_BASE_TYPE;
        callstats = &fincomm_call_data.Fuse[index];
    }
    else {
        index = (unsigned)message->Stats.RequestType.Native;
        assert((index >= FINESSE_NATIVE_REQ_BASE) && (index < FINESSE_NATIVE_REQ_MAX));
        index -= FINESSE_NATIVE_REQ_BASE;
        callstats = &fincomm_call_data.Native[index];
    }

    // static inline void timespec_diff(struct timespec * begin, struct timespec * end, struct timespec * diff)
    // static inline void timespec_add(struct timespec * one, struct timespec * two, struct timespec * result)

    pthread_mutex_lock(&callstats->Lock);
    if (0 == message->Result) {
        callstats->Success++;
        timespec_add(&callstats->SuccessTimings.RequestCreateTime, &message->Stats.RequestQueueTime,
                     &message->Stats.RequestStartTime);
        timespec_add(&callstats->SuccessTimings.RequestQueueDelay, &message->Stats.RequestDequeueTime,
                     &message->Stats.RequestQueueTime);
        timespec_add(&callstats->SuccessTimings.ResponseProcessingTime, &message->Stats.ResponseQueueTime,
                     &message->Stats.RequestDequeueTime);
        timespec_add(&callstats->SuccessTimings.ResponseQueueDelay, &message->Stats.ResponseReceiptTime,
                     &message->Stats.ResponseQueueTime);
        timespec_add(&callstats->SuccessTimings.RequestTotalTime, &message->Stats.RequestCompeltionTime,
                     &message->Stats.RequestStartTime);
    }
    else {
        callstats->Failure++;
        timespec_add(&callstats->FailureTimings.RequestCreateTime, &message->Stats.RequestQueueTime,
                     &message->Stats.RequestStartTime);
        timespec_add(&callstats->FailureTimings.RequestQueueDelay, &message->Stats.RequestDequeueTime,
                     &message->Stats.RequestQueueTime);
        timespec_add(&callstats->FailureTimings.ResponseProcessingTime, &message->Stats.ResponseQueueTime,
                     &message->Stats.RequestDequeueTime);
        timespec_add(&callstats->FailureTimings.ResponseQueueDelay, &message->Stats.ResponseReceiptTime,
                     &message->Stats.ResponseQueueTime);
        timespec_add(&callstats->FailureTimings.RequestTotalTime, &message->Stats.RequestCompeltionTime,
                     &message->Stats.RequestStartTime);
    }
    callstats->Calls++;
    pthread_mutex_unlock(&callstats->Lock);
}

#if 0
struct timespec RequestStartTime;
struct timespec RequestQueueTime;
struct timespec RequestDequeueTime;
struct timespec ResponseQueueTime;
struct timespec ResponseReceiptTime;
struct timespec RequestCompeltionTime;
#endif  // 0

void FincommCallStatRequestStart(fincomm_message Message)
{
    finesse_msg *message = (finesse_msg *)Message->Data;
    int          status;

    assert(NULL != Message);

    if (FINESSE_REQUEST != Message->MessageType) {
        return;  // not of interest - shouldn't really be called.
    }

    status = clock_gettime(CLOCK_MONOTONIC_RAW, &message->Stats.RequestStartTime);
    assert(0 == status);

    // Capture the request type here.
    message->Stats.RequestClass = message->MessageClass;
    switch (message->MessageClass) {
        case FINESSE_FUSE_MESSAGE:
            message->Stats.RequestType.Fuse = message->Message.Fuse.Request.Type;
            break;
        case FINESSE_NATIVE_MESSAGE:
            message->Stats.RequestType.Native = message->Message.Native.Request.NativeRequestType;
            break;
        default:
            assert(0);  // Say what?
            break;
    }
}

void FincommCallStatQueueRequest(fincomm_message Message)
{
    finesse_msg *message = (finesse_msg *)Message->Data;
    int          status;

    assert(NULL != Message);

    status = clock_gettime(CLOCK_MONOTONIC_RAW, &message->Stats.RequestQueueTime);
    assert(0 == status);
}

void FincommCallStatDequeueRequest(fincomm_message Message)
{
    finesse_msg *message = (finesse_msg *)Message->Data;
    int          status;

    assert(NULL != Message);

    status = clock_gettime(CLOCK_MONOTONIC_RAW, &message->Stats.RequestDequeueTime);
    assert(0 == status);
}

void FincommCallStatQueueResponse(fincomm_message Message)

{
    finesse_msg *message = (finesse_msg *)Message->Data;
    int          status;

    assert(NULL != Message);

    status = clock_gettime(CLOCK_MONOTONIC_RAW, &message->Stats.ResponseQueueTime);
    assert(0 == status);
}

void FincommCallStatDequeueResponse(fincomm_message Message)
{
    finesse_msg *message = (finesse_msg *)Message->Data;
    int          status;

    assert(NULL != Message);

    status = clock_gettime(CLOCK_MONOTONIC_RAW, &message->Stats.ResponseReceiptTime);
    assert(0 == status);
}
void FincommCallStatCompleteRequest(fincomm_message Message)
{
    finesse_msg *message = (finesse_msg *)Message->Data;
    int          status;

    assert(NULL != Message);

    status = clock_gettime(CLOCK_MONOTONIC_RAW, &message->Stats.RequestCompeltionTime);
    assert(0 == status);
}
