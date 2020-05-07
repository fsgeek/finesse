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