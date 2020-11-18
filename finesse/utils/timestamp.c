/*
 * Copyright (c) 2020 Tony Mason
 * All rights reserved.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "timestamp.h"

int FinesseGenerateTimestamp(char *TimestampBuffer, size_t TimestampBufferLength)
{
    time_t now;
    char   timestamp[32];

    timestamp[0] = '\0';
    assert(TimestampBufferLength >= 26);  // documented minimum length

    now = time(NULL);

    if (NULL == ctime_r(&now, timestamp)) {
        assert(0);
        return -1;  // this is a failure
    }

    if ('\n' == timestamp[strlen(timestamp) - 1]) {
        timestamp[strlen(timestamp) - 1] = '\0';  // we don't want an embedded newline
    }

    assert(strlen(timestamp) < TimestampBufferLength);
    memcpy(TimestampBuffer, timestamp, strlen(timestamp) + 1);

    return 0;
}
