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
    size_t request_length;
    Finesse__FinesseRequest *finesse_req;
    int status;

    status = FinesseGetRequest(ServerHandle, &request, &request_length);

    if (0 > status)
    {
        perror("FinesseGetRequest");
        return;
    }

    finesse_req = finesse__finesse_request__unpack(NULL, request_length, request);

    switch (finesse_req->header->op)
    {
    default: // Unknown case
        break;
    case FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__TEST:
        break;
    case FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__NAME_MAP:
        break;
    case FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__NAME_MAP_RELEASE:
        break;
    case FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__DIR_MAP:
        break;
    case FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__DIR_MAP_RELEASE:
        break;
    case FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__UNLINK:
        break;
    case FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__PATH_SEARCH:
        break;
    case FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__STATFS:
    	break;
    case FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__FSTATFS:
    	break;
    }

    // cleanup:
    if (NULL != finesse_req)
    {
        finesse__finesse_request__free_unpacked(finesse_req, NULL);
        finesse_req = NULL;
    }

    if (NULL != request)
    {
        FinesseFreeRequest(ServerHandle, request);
        request = NULL;
    }
}
