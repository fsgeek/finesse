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
struct timespec RequestCompletionTime;
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

// Note: this code is copied from callstats.h - probably should extract into a common header

static inline void timespec_diff(struct timespec *begin, struct timespec *end, struct timespec *diff)
{
    struct timespec result = {.tv_sec = 0, .tv_nsec = 0};

    // I've seen cases where the timestamps are just wrong; in that case, we should ignore the computation
    // (there's a test case that triggers this, where the message is created and then released...)
    if ((end->tv_sec > begin->tv_sec) || ((end->tv_sec == begin->tv_sec) && end->tv_nsec >= begin->tv_nsec)) {
        result.tv_sec = end->tv_sec - begin->tv_sec;
        if (end->tv_nsec < begin->tv_nsec) {
            result.tv_sec--;
            result.tv_nsec = (long)1000000000 + end->tv_nsec - begin->tv_nsec;
        }
    }
    *diff = result;
}

static inline void timespec_add_diff(struct timespec *accumulator, struct timespec *start, struct timespec *end)
{
    struct timespec diff;

    timespec_diff(start, end, &diff);
    accumulator->tv_sec += diff.tv_sec;
    accumulator->tv_nsec += diff.tv_nsec;
    while ((long)1000000000 <= accumulator->tv_nsec) {
        accumulator->tv_sec++;
        accumulator->tv_nsec -= (long)1000000000;
    }
}

static int is_zero_time(struct timespec *time)
{
    if ((time->tv_sec == 0) && (time->tv_nsec == 0)) {
        return 1;
    }
    return 0;
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
    struct timespec                diff;

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
        if ((index < FINESSE_NATIVE_REQ_BASE_TYPE) || (index >= FINESSE_NATIVE_REQ_MAX)) {
            fprintf(stderr, "Stats type (%u) is not valid\n", index);
        }
        assert((index >= FINESSE_NATIVE_REQ_BASE_TYPE) && (index < FINESSE_NATIVE_REQ_MAX));
        index -= FINESSE_NATIVE_REQ_BASE_TYPE;
        callstats = &fincomm_call_data.Native[index];
    }

    // static inline void timespec_diff(struct timespec * begin, struct timespec * end, struct timespec * diff)
    // static inline void timespec_add(struct timespec * one, struct timespec * two, struct timespec * result)

    pthread_mutex_lock(&callstats->Lock);
    assert(message->Stats.RequestStartTime.tv_sec > 0);  // if it is zero, it's probably not been set
    timespec_diff(&message->Stats.RequestStartTime, &message->Stats.RequestCompletionTime, &diff);
    assert(diff.tv_sec <= 3600);  // one hour?  Probably bogus data!

    if (0 == message->Result) {
        callstats->Success++;
        if (!is_zero_time(&message->Stats.RequestStartTime) && !is_zero_time(&message->Stats.RequestQueueTime)) {
            timespec_add_diff(&callstats->SuccessTimings.RequestCreateTime, &message->Stats.RequestStartTime,
                              &message->Stats.RequestQueueTime);
        }

        if (!is_zero_time(&message->Stats.RequestQueueTime) && !is_zero_time(&message->Stats.RequestDequeueTime)) {
            timespec_add_diff(&callstats->SuccessTimings.RequestQueueDelay, &message->Stats.RequestQueueTime,
                              &message->Stats.RequestDequeueTime);
        }

        if (!is_zero_time(&message->Stats.RequestDequeueTime) && !is_zero_time(&message->Stats.ResponseQueueTime)) {
            timespec_add_diff(&callstats->SuccessTimings.ResponseProcessingTime, &message->Stats.RequestDequeueTime,
                              &message->Stats.ResponseQueueTime);
        }

        if (!is_zero_time(&message->Stats.ResponseQueueTime) && !is_zero_time(&message->Stats.ResponseReceiptTime)) {
            timespec_add_diff(&callstats->SuccessTimings.ResponseQueueDelay, &message->Stats.ResponseQueueTime,
                              &message->Stats.ResponseReceiptTime);
        }

        if (!is_zero_time(&message->Stats.RequestStartTime) && !is_zero_time(&message->Stats.RequestCompletionTime)) {
            timespec_add_diff(&callstats->SuccessTimings.RequestTotalTime, &message->Stats.RequestStartTime,
                              &message->Stats.RequestCompletionTime);
        }
    }
    else {
        callstats->Failure++;
        if (!is_zero_time(&message->Stats.RequestStartTime) && !is_zero_time(&message->Stats.RequestQueueTime)) {
            timespec_add_diff(&callstats->FailureTimings.RequestCreateTime, &message->Stats.RequestStartTime,
                              &message->Stats.RequestQueueTime);
        }

        if (!is_zero_time(&message->Stats.RequestQueueTime) && !is_zero_time(&message->Stats.RequestDequeueTime)) {
            timespec_add_diff(&callstats->FailureTimings.RequestQueueDelay, &message->Stats.RequestQueueTime,
                              &message->Stats.RequestDequeueTime);
        }

        if (!is_zero_time(&message->Stats.RequestDequeueTime) && !is_zero_time(&message->Stats.ResponseQueueTime)) {
            timespec_add_diff(&callstats->FailureTimings.ResponseProcessingTime, &message->Stats.RequestDequeueTime,
                              &message->Stats.ResponseQueueTime);
        }

        if (!is_zero_time(&message->Stats.ResponseQueueTime) && !is_zero_time(&message->Stats.ResponseReceiptTime)) {
            timespec_add_diff(&callstats->FailureTimings.ResponseQueueDelay, &message->Stats.ResponseQueueTime,
                              &message->Stats.ResponseReceiptTime);
        }

        if (!is_zero_time(&message->Stats.RequestStartTime) && !is_zero_time(&message->Stats.RequestCompletionTime)) {
            timespec_add_diff(&callstats->FailureTimings.RequestTotalTime, &message->Stats.RequestStartTime,
                              &message->Stats.RequestCompletionTime);
        }
    }
    callstats->Calls++;
    pthread_mutex_unlock(&callstats->Lock);
}

void FincommCallStatRequestStart(fincomm_message Message)
{
    finesse_msg *                message = (finesse_msg *)Message->Data;
    int                          status;
    static const struct timespec zero = {.tv_sec = 0, .tv_nsec = 0};

    assert(NULL != Message);

    assert(FINESSE_REQUEST == Message->MessageType);

    if (FINESSE_REQUEST != Message->MessageType) {
        return;  // not of interest - shouldn't really be called.
    }

    // Capture the request type here.
    message->Stats.RequestClass = message->MessageClass;
    switch (message->MessageClass) {
        case FINESSE_FUSE_MESSAGE:
            message->Stats.RequestType.Fuse = message->Message.Fuse.Request.Type;
            assert((message->Stats.RequestType.Fuse >= FINESSE_FUSE_REQ_BASE_TYPE) &&
                   (message->Stats.RequestType.Fuse < FINESSE_FUSE_REQ_MAX));
            break;
        case FINESSE_NATIVE_MESSAGE:
            message->Stats.RequestType.Native = message->Message.Native.Request.NativeRequestType;
            assert(0 != message->Stats.RequestType.Native);
            assert((message->Stats.RequestType.Native >= FINESSE_NATIVE_REQ_BASE_TYPE) &&
                   (message->Stats.RequestType.Native < FINESSE_NATIVE_REQ_MAX));
            break;
        default:
            assert(0);  // Say what?
            break;
    }

    // Make sure to zero out the stats values
    message->Stats.RequestQueueTime = message->Stats.RequestDequeueTime = message->Stats.ResponseQueueTime =
        message->Stats.ResponseReceiptTime = message->Stats.RequestCompletionTime = zero;

    status = clock_gettime(CLOCK_MONOTONIC_RAW, &message->Stats.RequestStartTime);
    assert(0 == status);
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

    status = clock_gettime(CLOCK_MONOTONIC_RAW, &message->Stats.RequestCompletionTime);
    assert(0 == status);
}

#if 1
static void FincommFormatStatEntry(fincomm_api_call_statistics_t *Entry, char *Buffer, size_t *BufferSize, unsigned base)
{
    static const char *FormatString = "%u,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n";
    int                retval;
    unsigned long long sqdelay = Entry->SuccessTimings.RequestQueueDelay.tv_sec * (unsigned long long)1000000000 +
                                 Entry->SuccessTimings.RequestQueueDelay.tv_nsec;
    unsigned long long sproc = Entry->SuccessTimings.ResponseProcessingTime.tv_sec * (unsigned long long)1000000000 +
                               Entry->SuccessTimings.ResponseProcessingTime.tv_nsec;
    unsigned long long srspqdelay = Entry->SuccessTimings.ResponseQueueDelay.tv_sec * (unsigned long long)1000000000 +
                                    Entry->SuccessTimings.ResponseQueueDelay.tv_nsec;
    unsigned long long stotal = Entry->SuccessTimings.RequestTotalTime.tv_sec * (unsigned long long)1000000000 +
                                Entry->SuccessTimings.RequestTotalTime.tv_nsec;

    unsigned long long fqdelay = Entry->FailureTimings.RequestQueueDelay.tv_sec * (unsigned long long)1000000000 +
                                 Entry->FailureTimings.RequestQueueDelay.tv_nsec;
    unsigned long long fproc = Entry->FailureTimings.ResponseProcessingTime.tv_sec * (unsigned long long)1000000000 +
                               Entry->FailureTimings.ResponseProcessingTime.tv_nsec;
    unsigned long long frspqdelay = Entry->FailureTimings.ResponseQueueDelay.tv_sec * (unsigned long long)1000000000 +
                                    Entry->FailureTimings.ResponseQueueDelay.tv_nsec;
    unsigned long long ftotal = Entry->FailureTimings.RequestTotalTime.tv_sec * (unsigned long long)1000000000 +
                                Entry->FailureTimings.RequestTotalTime.tv_nsec;

    retval = snprintf(Buffer, *BufferSize, FormatString, base, Entry->Calls, Entry->Success, sqdelay, sproc, srspqdelay, stotal,
                      Entry->Failure, fqdelay, fproc, frspqdelay, ftotal);

    if (retval >= 0) {
        *BufferSize = (size_t)retval;
    }
    else {
        *BufferSize = 250;  // SWAG
    }
}
#else
static double TimespecToFloat(struct timespec *time)
{
    double seconds = time->tv_nsec;

    seconds /= (double)(1000000000);
    seconds += (double)time->tv_sec;

    return seconds;
}

static void FincommFormatStatEntry(fincomm_api_call_statistics_t *Entry, char *Buffer, size_t *BufferSize, unsigned base)
{
    static const char *FormatString = "%u,%lu,%lu,%.9f,%.9f,%.9f,%.9f,%lu,%.9f,%.9f,%.9f,%.9f\n";
    int                retval;
    double             sqdelay    = TimespecToFloat(&Entry->SuccessTimings.RequestQueueDelay);
    double             sproc      = TimespecToFloat(&Entry->SuccessTimings.ResponseProcessingTime);
    double             srspqdelay = TimespecToFloat(&Entry->SuccessTimings.ResponseQueueDelay);
    double             stotal     = TimespecToFloat(&Entry->SuccessTimings.RequestTotalTime);
    double             fqdelay    = TimespecToFloat(&Entry->FailureTimings.RequestQueueDelay);
    double             fproc      = TimespecToFloat(&Entry->FailureTimings.ResponseProcessingTime);
    double             frspqdelay = TimespecToFloat(&Entry->FailureTimings.ResponseQueueDelay);
    double             ftotal     = TimespecToFloat(&Entry->FailureTimings.RequestTotalTime);

    retval = snprintf(Buffer, *BufferSize, FormatString, base, Entry->Calls, Entry->Success, sqdelay, sproc, srspqdelay, stotal,
                      Entry->Failure, fqdelay, fproc, frspqdelay, ftotal);

    if (retval >= 0) {
        *BufferSize = (size_t)retval;
    }
    else {
        *BufferSize = 250;  // SWAG
    }
}
#endif  // 1

static const char *fincomm_stat_log_default       = "default";
static const char *fincomm_stat_log_dir_default   = "tmp";
static const char *fincomm_stat_log_env           = "FINESSE_COMM_STAT_LOG";
static const char *fincomm_stat_log_dir_env       = "FINESSE_COMM_STAT_LOG_DIR";
static const char *fincomm_stat_log_file_template = "/%s/finesse-commstats-%s-%s-%d.log";

void FincommSaveStats(const char *Timestamp)
{
    int                fd = -1;
    static const char *CsvHeaderString =
        "Operation,Calls,Success,RequestQueueDelay,Processing,ResponseQueueDelay,TotalTime,Failure,RequestQueueDelay,Processing,"
        "ResponseQueueDelay,TotalTime\n";
    char        buffer[1024];
    size_t      buffer_size;
    char        local_timestamp[64];
    const char *timestamp;
    const char *log_name = getenv(fincomm_stat_log_env);
    const char *log_dir  = getenv(fincomm_stat_log_dir_env);
    char        fincomm_stat_log[256];
    int         retval;

    if (NULL == Timestamp) {
        timestamp = local_timestamp;
        retval    = FinesseGenerateTimestamp(local_timestamp, sizeof(local_timestamp));
        assert(0 == retval);
    }
    else {
        timestamp = Timestamp;
    }

    if ((NULL == log_name) || strlen(log_name) >= sizeof(fincomm_stat_log)) {
        log_name = fincomm_stat_log_default;
    }

    if ((NULL == log_dir) || strlen(log_dir) >= sizeof(fincomm_stat_log)) {
        log_dir = fincomm_stat_log_dir_default;
    }

    retval = snprintf(fincomm_stat_log, sizeof(fincomm_stat_log), fincomm_stat_log_file_template, log_dir, log_name, timestamp,
                      getpid());

    assert((size_t)retval < sizeof(fincomm_stat_log));

    fd = open(fincomm_stat_log, O_CREAT | O_EXCL | O_RDWR, 0644);

    assert(fd >= 0);

    if (fd < 0) {
        return;
    }

    // Write the CSV header
    retval = write(fd, CsvHeaderString, strlen(CsvHeaderString));
    assert(retval >= 0);

    // First, write the FUSE data
    for (unsigned index = 0; index < FINESSE_FUSE_REQ_MAX - FINESSE_FUSE_REQ_LOOKUP; index++) {
        buffer_size = sizeof(buffer);
        FincommFormatStatEntry(&fincomm_call_data.Fuse[index], buffer, &buffer_size, index + FINESSE_FUSE_REQ_BASE_TYPE);
        assert(buffer_size < sizeof(buffer));
        retval = write(fd, buffer, strlen(buffer));
        assert(retval >= 0);
    }

    // Now, write the native data
    for (unsigned index = 0; index < FINESSE_NATIVE_REQ_MAX - FINESSE_NATIVE_REQ_TEST; index++) {
        buffer_size = sizeof(buffer);
        FincommFormatStatEntry(&fincomm_call_data.Native[index], buffer, &buffer_size, index + FINESSE_NATIVE_REQ_BASE_TYPE);
        assert(buffer_size < sizeof(buffer));
        retval = write(fd, buffer, strlen(buffer));
        assert(retval >= 0);
    }

    fsync(fd);
    close(fd);
}
