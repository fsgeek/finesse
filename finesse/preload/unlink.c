/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

#include <finesse.h>
#include "preload.h"


int unlink(const char *pathname)
{
    return finesse_unlink(pathname);
}

int unlinkat(int dirfd, const char *pathname, int flags)
{
    return finesse_unlinkat(dirfd, pathname, flags);
}
