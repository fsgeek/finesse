//
// (C) Copyright 2020 Tony Mason
// All Right Reserved
//

#if !defined(__FINESSE_CALLSTATS_H__)
#define __FINESSE_CALLSTATS_H__ (1)
#include <assert.h>
#include <stdint.h>
#include <time.h>

#define FINESSE_API_CALL_BASE (200)
#define FINESSE_API_CALL_ACCESS (FINESSE_API_CALL_BASE + 1)
#define FINESSE_API_CALL_FACCESSAT (FINESSE_API_CALL_ACCESS + 1)
#define FINESSE_API_CALL_CHMOD (FINESSE_API_CALL_FACCESSAT + 1)
#define FINESSE_API_CALL_CHOWN (FINESSE_API_CALL_CHMOD + 1)
#define FINESSE_API_CALL_CLOSE (FINESSE_API_CALL_CHOWN + 1)
#define FINESSE_API_CALL_DIR (FINESSE_API_CALL_CLOSE + 1)
#define FINESSE_API_CALL_DUP (FINESSE_API_CALL_DIR + 1)
#define FINESSE_API_CALL_FSTAT (FINESSE_API_CALL_DUP + 1)
#define FINESSE_API_CALL_FSTATAT (FINESSE_API_CALL_FSTAT + 1)
#define FINESSE_API_CALL_LSTAT (FINESSE_API_CALL_FSTATAT + 1)
#define FINESSE_API_CALL_LINK (FINESSE_API_CALL_LSTAT + 1)
#define FINESSE_API_CALL_LSEEK (FINESSE_API_CALL_LINK + 1)
#define FINESSE_API_CALL_MKDIR (FINESSE_API_CALL_LSEEK + 1)
#define FINESSE_API_CALL_OPEN (FINESSE_API_CALL_MKDIR + 1)
#define FINESSE_API_CALL_READ (FINESSE_API_CALL_OPEN + 1)
#define FINESSE_API_CALL_RENAME (FINESSE_API_CALL_READ + 1)
#define FINESSE_API_CALL_RMDIR (FINESSE_API_CALL_RENAME + 1)
#define FINESSE_API_CALL_STAT (FINESSE_API_CALL_RMDIR + 1)
#define FINESSE_API_CALL_STATX (FINESSE_API_CALL_STAT + 1)
#define FINESSE_API_CALL_UNLINK (FINESSE_API_CALL_STATX + 1)
#define FINESSE_API_CALL_UTIME (FINESSE_API_CALL_UNLINK + 1)
#define FINESSE_API_CALL_WRITE (FINESSE_API_CALL_UTIME + 1)
#define FINESSE_API_CALLS_MAX (FINESSE_API_CALL_WRITE + 1)
#define FINESSE_API_CALLS_COUNT \
  (FINESSE_API_CALLS_MAX - (FINESSE_API_CALL_BASE + 1))

typedef struct _finessse_api_call_statistics {
  uint64_t Calls;
  uint64_t Success;
  uint64_t Failure;
  struct timespec LibraryElapsedTime;
  struct timespec NativeElapsedTime;
  char const *Name;
} finesse_api_call_statistics_t;

void FinesseApiInitializeCallStatistics(void);
finesse_api_call_statistics_t *FinesseApiGetCallStatistics(void);
void FinesseApiReleaseCallStatistics(
    finesse_api_call_statistics_t *CallStatistics);
const char *FinesseApiFormatCallData(finesse_api_call_statistics_t *CallData,
                                     int CsvFormat);
void FinesseApiFreeFormattedCallData(const char *FormattedData);
void FinesseApiFormatCallDataEntry(finesse_api_call_statistics_t *CallDataEntry,
                                   int CsvFormat, char *Buffer,
                                   size_t *BufferSize);

static inline void timespec_diff(struct timespec *begin, struct timespec *end,
                                 struct timespec *diff) {
  struct timespec result = {.tv_sec = 0, .tv_nsec = 0};
  assert((end->tv_sec > begin->tv_sec) ||
         ((end->tv_sec == begin->tv_sec) && end->tv_nsec >= begin->tv_nsec));
  result.tv_sec = end->tv_sec - begin->tv_sec;
  if (end->tv_nsec < begin->tv_nsec) {
    result.tv_sec--;
    result.tv_nsec = (long)1000000000 + end->tv_nsec - begin->tv_nsec;
  }
  *diff = result;
}

static inline void timespec_add(struct timespec *one, struct timespec *two,
                                struct timespec *result) {
  result->tv_sec = one->tv_sec + two->tv_sec;
  result->tv_nsec = one->tv_nsec + two->tv_nsec;
  while ((long)1000000000 <= result->tv_nsec) {
    result->tv_sec++;
    result->tv_nsec -= (long)1000000000;
  }
}

void FinesseApiCountCall(uint8_t Call, uint8_t Success);
void FinesseApiRecordNative(uint8_t Call, struct timespec *Elapsed);
void FinesseApiRecordOverhead(uint8_t Call, struct timespec *Elapsed);

#endif  // __FINESSE_CALLSTATS_H__
