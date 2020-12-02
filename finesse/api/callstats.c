
//
// (C) Copyright 2020 Tony Mason
// All Right Reserved
//
//

#include <fcntl.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "callstats.h"
#include "timestamp.h"

static finesse_api_call_statistics_t FinesseApiCallStatistics[FINESSE_API_CALLS_COUNT];

static const char *FinesseCallDataNames[] = {"Access", "Faccessat", "Chdir",  "Chmod",   "Chown",  "Close",    "Creat",   "Dir",
                                             "Dup",    "Fopen",     "Fdopen", "Freopen", "Fstat",  "Fstatat",  "Fstatfs", "Lstat",
                                             "Link",   "Lseek",     "Mkdir",  "Mkdirat", "Open",   "Openat",   "Read",    "Rename",
                                             "Rmdir",  "Stat",      "Statx",  "Statfs",  "Unlink", "Unlinkat", "Utime",   "Write"};

static const char *FinesseCallDataNames[FINESSE_API_CALLS_COUNT];

void FinesseApiInitializeCallStatistics(void)
{
    _Static_assert((sizeof(FinesseApiCallStatistics) / sizeof(finesse_api_call_statistics_t)) ==
                       (sizeof(FinesseCallDataNames) / sizeof(const char *)),
                   "Incorrect number of names");
    for (unsigned index = 0; index < sizeof(FinesseApiCallStatistics) / sizeof(finesse_api_call_statistics_t); index++) {
        assert(NULL != FinesseCallDataNames[index]);
        FinesseApiCallStatistics[index].Name = FinesseCallDataNames[index];
    }
}

finesse_api_call_statistics_t *FinesseApiGetCallStatistics(void)
{
    finesse_api_call_statistics_t *copy = (finesse_api_call_statistics_t *)malloc(sizeof(FinesseApiCallStatistics));

    if (NULL != copy) {
        memcpy(copy, FinesseApiCallStatistics, sizeof(FinesseApiCallStatistics));
    }
    return copy;
}

void FinesseApiReleaseCallStatistics(finesse_api_call_statistics_t *CallStatistics)
{
    if (NULL != CallStatistics) {
        free(CallStatistics);
    }
}

void FinesseApiCountCall(uint8_t Call, uint8_t Success)
{
    assert((Call > FINESSE_API_CALL_BASE) && (Call < FINESSE_API_CALLS_MAX));
    assert((0 == Success) || (1 == Success));
    FinesseApiCallStatistics[Call - (FINESSE_API_CALL_BASE + 1)].Calls++;
    if (Success) {
        FinesseApiCallStatistics[Call - (FINESSE_API_CALL_BASE + 1)].Success++;
    }
    else {
        FinesseApiCallStatistics[Call - (FINESSE_API_CALL_BASE + 1)].Failure++;
    }
}

void FinesseApiRecordNative(uint8_t Call, struct timespec *Elapsed)
{
    assert((Call > FINESSE_API_CALL_BASE) && (Call < FINESSE_API_CALLS_MAX));
    timespec_add(&FinesseApiCallStatistics[Call - (FINESSE_API_CALL_BASE + 1)].NativeElapsedTime, Elapsed,
                 &FinesseApiCallStatistics[Call - (FINESSE_API_CALL_BASE + 1)].NativeElapsedTime);
}

void FinesseApiRecordOverhead(uint8_t Call, struct timespec *Elapsed)
{
    assert((Call > FINESSE_API_CALL_BASE) && (Call < FINESSE_API_CALLS_MAX));
    timespec_add(&FinesseApiCallStatistics[Call - (FINESSE_API_CALL_BASE + 1)].LibraryElapsedTime, Elapsed,
                 &FinesseApiCallStatistics[Call - (FINESSE_API_CALL_BASE + 1)].LibraryElapsedTime);
}

// Given a copy of the call data, this routine will create a single string suitable for printing
// the data.  If the CallData parameter is NULL, the default CallData will be used.
const char *FinesseApiFormatCallData(finesse_api_call_statistics_t *CallData, int CsvFormat)
{
    finesse_api_call_statistics_t *cd              = CallData;
    int                            allocated       = 0;
    size_t                         required_space  = 0;
    char *                         formatted_data  = NULL;
    static const char *            CsvHeaderString = "Routine, Invocations, Succeeded, Failed, Native:Elapsed, Library:Elapsed\n";

    if (NULL == CallData) {
        cd        = FinesseApiGetCallStatistics();
        allocated = 1;
    }

    if (CsvFormat) {
        // string + 1 for null + 7 (and mask) for round-up
        required_space = (sizeof(CsvHeaderString) + 8) & (~7);
    }

    for (unsigned index = 0; index < sizeof(FinesseApiCallStatistics) / sizeof(finesse_api_call_statistics_t); index++) {
        char   buffer[16];
        size_t bufsize = sizeof(buffer);

        FinesseApiFormatCallDataEntry(&cd[index], CsvFormat, buffer, &bufsize);
        required_space += (bufsize + 7) & (~7);  // round up to 8 bytes.
    }

    formatted_data = (char *)malloc(required_space);

    if (NULL != formatted_data) {
        size_t space_used = 0;

        for (unsigned index = 0; index < sizeof(FinesseApiCallStatistics) / sizeof(finesse_api_call_statistics_t); index++) {
            size_t space_for_entry = required_space - space_used;
            size_t entry_space     = space_for_entry;

            FinesseApiFormatCallDataEntry(&cd[index], CsvFormat, &formatted_data[space_used], &space_for_entry);
            space_used += space_for_entry;
            assert(entry_space >= space_for_entry);  // make sure we didn't overflow...
            assert(space_used <= required_space);
        }
    }

    if (allocated) {
        FinesseApiReleaseCallStatistics(cd);
        cd = NULL;
    }

    return formatted_data;
}

void FinesseApiFreeFormattedCallData(const char *FormattedData)
{
    if (NULL != FormattedData) {
        free((void *)FormattedData);
    }
}

// Given a single call entry, this function will format it into the provided buffer up to the amount
// specified in BufferSize.  The returned value of BufferSize is the size required to store the entry.
// Note that the entry is one line, with a newline character at the end.
void FinesseApiFormatCallDataEntry(finesse_api_call_statistics_t *CallDataEntry, int CsvFormat, char *Buffer, size_t *BufferSize)
{
    uint64_t lib_nsec = CallDataEntry->LibraryElapsedTime.tv_sec * (unsigned long)1000000000 +
                        (unsigned long)CallDataEntry->LibraryElapsedTime.tv_nsec;
    uint64_t nat_nsec = CallDataEntry->NativeElapsedTime.tv_sec * (unsigned long)1000000000 +
                        (unsigned long)CallDataEntry->NativeElapsedTime.tv_nsec;
#pragma GCC diagnostic push
#if !defined(__clang__)
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif  // __clang__
    // It fusses about the fact that this might truncate the value of... something.  Don't care, this is
    // diagnostic code!
    static const char *FormatString =
        "%16s: %10lu Calls (%10lu Success, %10lu Failure), Elapsed = %16lu (ns), Average = %16.2f (ns)\n";
    static const char *CsvFormatString = " %s, %lu, %lu, %lu, %lu, %.2f\n";
#pragma GCC diagnostic pop
    int retval;

    if (CsvFormat) {
        retval = snprintf(Buffer, *BufferSize, CsvFormatString, CallDataEntry->Name, CallDataEntry->Calls, CallDataEntry->Success,
                          CallDataEntry->Failure, nat_nsec, lib_nsec);
    }
    else {
        retval = snprintf(Buffer, *BufferSize, FormatString, CallDataEntry->Name, CallDataEntry->Calls, CallDataEntry->Success,
                          CallDataEntry->Failure, nat_nsec, lib_nsec);
    }

    if (retval >= 0) {
        *BufferSize = (size_t)retval;
    }
    else {
        *BufferSize = 150;
    }
}
