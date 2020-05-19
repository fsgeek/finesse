/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

#include <finesse.h>
#include "preload.h"


int unlink(const char *pathname)
{
    finesse_preload_init();

    return finesse_unlink(pathname);
}

int finesse_unlinkat(int dirfd, const char *pathname, int flags)
{
    finesse_preload_init();

    return finesse_unlinkat(dirfd, pathname, flags);
}
