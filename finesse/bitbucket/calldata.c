
//
// (C) Copyright 2020 Tony Mason
// All Right Reserved
//
//

#include "bitbucketcalls.h"
#include <malloc.h>
#include <string.h>

static bitbucket_call_statistics_t BitbucketCallStatistics[44];

static  const char * BitbucketCallDataNames[] = {
	"Init",
	"Destroy",
	"Lookup",
	"Forget",
	"Getattr",
	"Setattr",
	"Readlink",
	"Mknod",
	"Mkdir",
	"Unlink",
	"Rmdir",
	"Symlink",
	"Rename",
	"Link",
	"Open",
	"Read",
	"Write",
	"Flush",
	"Release",
	"Fsync",
	"Opendir",
	"Readdir",
	"Releasedir",
	"Fsyncdir",
	"Statfs",
	"Setxattr",
	"Getxattr",
	"Listxattr",
	"Removexattr",
	"Access",
	"Create",
	"Getlk",
	"Setlk",
	"Bmap",
	"Ioctl",
	"Poll",
	"Write_Buf",
	"Retrieve_Reply",
	"Forget_Multi",
	"Flock",
	"Fallocate",
	"Readdirplus",
	"Copy_File_Range",
	"Lseek",
	NULL,
};

void BitbucketInitializeCallStatistics(void)
{
	_Static_assert((sizeof(BitbucketCallStatistics) / sizeof(bitbucket_call_statistics_t)) <= 
	(sizeof(BitbucketCallDataNames) / sizeof(const char *)), "Too few names");
	for (unsigned index = 0; index < sizeof(BitbucketCallStatistics) / sizeof(bitbucket_call_statistics_t); index++) {
		assert(NULL != BitbucketCallDataNames[index]);
		BitbucketCallStatistics[index].Name = BitbucketCallDataNames[index];
	}
}

bitbucket_call_statistics_t *BitbucketGetCallStatistics(void)
{
	bitbucket_call_statistics_t *copy = (bitbucket_call_statistics_t *)malloc(sizeof(BitbucketCallStatistics));

	if (NULL != copy) {
		memcpy(copy, BitbucketCallStatistics, sizeof(BitbucketCallStatistics));
	}
	return copy;
}

void BitbucketReleaseCallStatistics(bitbucket_call_statistics_t *CallStatistics)
{
	if (NULL != CallStatistics) {
		free(CallStatistics);
	}
}

void BitbucketCountCall(uint8_t Call, uint8_t Success, struct timespec *Elapsed) {
	assert((Call > BITBUCKET_CALL_BASE) && (Call < BITBUCKET_CALLS_MAX));
	assert((0 == Success) || (1 == Success));
	BitbucketCallStatistics[Call - BITBUCKET_CALL_BASE].Calls++;
	BitbucketCallStatistics[Call - BITBUCKET_CALL_BASE].Success += Success;
	BitbucketCallStatistics[Call - BITBUCKET_CALL_BASE].Failure += !Success;
	timespec_add(&BitbucketCallStatistics[Call - BITBUCKET_CALL_BASE].ElapsedTime, Elapsed, &BitbucketCallStatistics[Call - BITBUCKET_CALL_BASE].ElapsedTime);
}

// Given a copy of the call data, this routine will create a single string suitable for printing
// the data.  If the CallData parameter is NULL, the default CallData will be used.
const char *BitbucketFormatCallData(bitbucket_call_statistics_t *CallData)
{
	bitbucket_call_statistics_t *cd = CallData;
	int allocated = 0;
	size_t required_space = 0;
	char *formatted_data = NULL;

	if (NULL == CallData) {
		cd = BitbucketGetCallStatistics();
		allocated = 1;
	}

	for (unsigned index = 0; index < sizeof(BitbucketCallStatistics) / sizeof(bitbucket_call_statistics_t); index++) {
		char buffer[16];
		size_t bufsize = sizeof(buffer);

		BitbucketFormatCallDataEntry(&cd[index], buffer, &bufsize);
		required_space += (bufsize + 7) & (~7); // round up to 8 bytes.
	}

	formatted_data = (char *)malloc(required_space);
	
	if (NULL != formatted_data) {
		size_t space_used = 0;

		for (unsigned index = 0; index < sizeof(BitbucketCallStatistics) / sizeof(bitbucket_call_statistics_t); index++) {
			size_t space_for_entry = required_space - space_used;
			size_t entry_space = space_for_entry;

			BitbucketFormatCallDataEntry(&cd[index], &formatted_data[space_used], &space_for_entry);
			space_used += space_for_entry;
			assert(entry_space >= space_for_entry); // make sure we didn't overflow...
			assert(space_used <= required_space);
		}
	}

	if (allocated) {
		BitbucketReleaseCallStatistics(cd);
		cd = NULL;
	}

	return formatted_data;

}

void BitbucketFreeFormattedCallData(const char *FormattedData)
{
	if (NULL != FormattedData) {
		free((void *)FormattedData);
	}
}


// Given a single call entry, this function will format it into the provided buffer up to the amount
// specified in BufferSize.  The returned value of BufferSize is the size required to store the entry.
// Note that the entry is one line, with a newline character at the end.
void BitbucketFormatCallDataEntry(bitbucket_call_statistics_t *CallDataEntry, char *Buffer, size_t *BufferSize)
{
	uint64_t nsec = CallDataEntry->ElapsedTime.tv_sec * (unsigned long)1000000000 + (unsigned long) CallDataEntry->ElapsedTime.tv_nsec;
	double average;
	static const char *FormatString = "%16s: %8lu Calls (%8lu Success, %8lu Failure), Elapsed = %16lu (ns), Average = %16.2f (ns)\n";
	int retval;

	if (CallDataEntry->Calls > 0) {
		average = (double) nsec / (double) CallDataEntry->Calls;
	}
	else {
		average = 0.0;
	}

	retval = snprintf(Buffer, 
					 *BufferSize, 
					  FormatString, 
					  CallDataEntry->Name,
					  CallDataEntry->Calls,
					  CallDataEntry->Success,
					  CallDataEntry->Failure,
					  nsec,
					  average);
	
	assert(retval >= 0);
	*BufferSize = (size_t)retval;

}
