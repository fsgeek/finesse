/*
 * Copyright (c) 2018, Tony Mason. All rights reserved.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "finesse.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <uuid/uuid.h>
#include <pthread.h>
#include <errno.h>
#include "finesse.pb-c.h"

void FinesseServerGetRequest(finesse_server_handle_t ServerHandle);

void FinesseServerGetRequest(finesse_server_handle_t ServerHandle)
{
    void *request;
    void *client;
    Finesse__FinesseRequest *finesse_req;
    int status;

    status = FinesseGetRequest(ServerHandle, &client, &request);

    if (0 > status)
    {
        perror("FinesseGetRequest");
        return;
    }

    assert(0); // This needs to be moved to the new communications package
    
}
