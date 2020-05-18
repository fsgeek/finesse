/*
  Copyright (C) 2020  Tony Mason <fsgeek@cs.ubc.ca>
*/
#include "config.h"
#include "fuse_i.h"
#include "fuse_kernel.h"
#include "fuse_opt.h"
#include "fuse_misc.h"
#include <fuse_lowlevel.h>
#include "finesse-fuse.h"
#include <finesse-server.h>


typedef struct finesse_lookup_info
{
    int status;

    /* used so the caller can wait for completion */
    pthread_mutex_t lock;
    pthread_cond_t condition;
    int completed;

    /* attributes */
    struct fuse_attr attr;

} finesse_lookup_info_t;



void FinesseServerInternalLookup(struct fuse_session *se, const char *Path);
