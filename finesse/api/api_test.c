/*
 * (C) Copyright 2018 Tony Mason
 * All Rights Reserved
 */

#include "api-internal.h"
#include "callstats.h"

//
// These are testing APIs, shouldn't be present in production builds (TODO)
//

//
// Given a file descriptor, tell the caller if it is being tracked by
// the Finesse library.
//
int finesse_is_fd_tracked(int fd)
{
    return (NULL != finesse_lookup_file_state(finesse_nfd_to_fd(fd)));
}
