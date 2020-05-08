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

#if 0
static int initialized = 0;

void fincomm_init(void)
{
    if (0 == initialized) {
        initialized = 1;
    }
    return;
}

void fincomm_shutdown(void)
{
    if (1 == initialized) {
        initialized = 0;
    }
    return;
}
#endif // 0