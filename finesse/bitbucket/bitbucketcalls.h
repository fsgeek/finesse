//
// (C) Copyright 2020 Tony Mason
// All Right Reserved
//

#if !defined(__BITBUCKET_CALLS_H__)
#define __BITBUCKET_CALLS_H__ (1)
#define BITBUCKET_CALL_BASE (100)
#include <assert.h>
#include <stdint.h>
#include <time.h>

#define BITBUCKET_CALL_INIT (BITBUCKET_CALL_BASE + 1)
#define BITBUCKET_CALL_DESTROY (BITBUCKET_CALL_BASE + 2)
#define BITBUCKET_CALL_LOOKUP (BITBUCKET_CALL_BASE + 3)
#define BITBUCKET_CALL_FORGET (BITBUCKET_CALL_BASE + 4)
#define BITBUCKET_CALL_GETATTR (BITBUCKET_CALL_BASE + 5)
#define BITBUCKET_CALL_SETATTR (BITBUCKET_CALL_BASE + 6)
#define BITBUCKET_CALL_READLINK (BITBUCKET_CALL_BASE + 7)
#define BITBUCKET_CALL_MKNOD (BITBUCKET_CALL_BASE + 8)
#define BITBUCKET_CALL_MKDIR (BITBUCKET_CALL_BASE + 9)
#define BITBUCKET_CALL_UNLINK (BITBUCKET_CALL_BASE + 10)
#define BITBUCKET_CALL_RMDIR (BITBUCKET_CALL_BASE + 11)
#define BITBUCKET_CALL_SYMLINK (BITBUCKET_CALL_BASE + 12)
#define BITBUCKET_CALL_RENAME (BITBUCKET_CALL_BASE + 13)
#define BITBUCKET_CALL_LINK (BITBUCKET_CALL_BASE + 14)
#define BITBUCKET_CALL_OPEN (BITBUCKET_CALL_BASE + 15)
#define BITBUCKET_CALL_READ (BITBUCKET_CALL_BASE + 16)
#define BITBUCKET_CALL_WRITE (BITBUCKET_CALL_BASE + 17)
#define BITBUCKET_CALL_FLUSH (BITBUCKET_CALL_BASE + 18)
#define BITBUCKET_CALL_RELEASE (BITBUCKET_CALL_BASE + 19)
#define BITBUCKET_CALL_FSYNC (BITBUCKET_CALL_BASE + 20)
#define BITBUCKET_CALL_OPENDIR (BITBUCKET_CALL_BASE + 21)
#define BITBUCKET_CALL_READDIR (BITBUCKET_CALL_BASE + 22)
#define BITBUCKET_CALL_RELEASEDIR (BITBUCKET_CALL_BASE + 23)
#define BITBUCKET_CALL_FSYNCDIR (BITBUCKET_CALL_BASE + 24)
#define BITBUCKET_CALL_STATFS (BITBUCKET_CALL_BASE + 25)
#define BITBUCKET_CALL_SETXATTR (BITBUCKET_CALL_BASE + 26)
#define BITBUCKET_CALL_GETXATTR (BITBUCKET_CALL_BASE + 27)
#define BITBUCKET_CALL_LISTXATTR (BITBUCKET_CALL_BASE + 28)
#define BITBUCKET_CALL_REMOVEXATTR (BITBUCKET_CALL_BASE + 29)
#define BITBUCKET_CALL_ACCESS (BITBUCKET_CALL_BASE + 30)
#define BITBUCKET_CALL_CREATE (BITBUCKET_CALL_BASE + 31)
#define BITBUCKET_CALL_GETLK (BITBUCKET_CALL_BASE + 32)
#define BITBUCKET_CALL_SETLK (BITBUCKET_CALL_BASE + 33)
#define BITBUCKET_CALL_BMAP (BITBUCKET_CALL_BASE + 34)
#define BITBUCKET_CALL_IOCTL (BITBUCKET_CALL_BASE + 35)
#define BITBUCKET_CALL_POLL (BITBUCKET_CALL_BASE + 36)
#define BITBUCKET_CALL_WRITE_BUF (BITBUCKET_CALL_BASE + 37)
#define BITBUCKET_CALL_RETRIEVE_REPLY (BITBUCKET_CALL_BASE + 38)
#define BITBUCKET_CALL_FORGET_MULTI (BITBUCKET_CALL_BASE + 39)
#define BITBUCKET_CALL_FLOCK (BITBUCKET_CALL_BASE + 40)
#define BITBUCKET_CALL_FALLOCATE (BITBUCKET_CALL_BASE + 41)
#define BITBUCKET_CALL_READDIRPLUS (BITBUCKET_CALL_BASE + 42)
#define BITBUCKET_CALL_COPY_FILE_RANGE (BITBUCKET_CALL_BASE + 43)
#define BITBUCKET_CALL_LSEEK (BITBUCKET_CALL_BASE + 44)
#define BITBUCKET_CALLS_MAX (144)

typedef struct _bitbucket_call_statistics {
    uint64_t        Calls;
    uint64_t        Success;
    uint64_t        Failure;
    struct timespec ElapsedTime;
    char const *    Name;
} bitbucket_call_statistics_t;

void                         BitbucketInitializeCallStatistics(void);
bitbucket_call_statistics_t *BitbucketGetCallStatistics(void);
void                         BitbucketReleaseCallStatistics(bitbucket_call_statistics_t *CallStatistics);
const char *                 BitbucketFormatCallData(bitbucket_call_statistics_t *CallData, int CsvFormat);
void                         BitbucketFreeFormattedCallData(const char *FormattedData);
void BitbucketFormatCallDataEntry(bitbucket_call_statistics_t *CallDataEntry, int CsvFormat, char *Buffer, size_t *BufferSize);

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

static inline void timespec_add(struct timespec *one, struct timespec *two, struct timespec *result)
{
    result->tv_sec  = one->tv_sec + two->tv_sec;
    result->tv_nsec = one->tv_nsec + two->tv_nsec;
    while ((long)1000000000 <= result->tv_nsec) {
        result->tv_sec++;
        result->tv_nsec -= (long)1000000000;
    }
}

void BitbucketCountCall(uint8_t Call, uint8_t Success, struct timespec *Elapsed);

#endif  // ____BITBUCKET_CALLS_H__
