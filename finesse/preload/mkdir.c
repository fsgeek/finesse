/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

#include <finesse.h>
#include "preload.h"

int mkdir(const char *path, mode_t mode) 
{
    finesse_preload_init();

    return finesse_mkdir(path, mode);
}

int mkdirat(int fd, const char *path, mode_t mode) 
{
    finesse_preload_init();

    return finesse_mkdirat(fd, path, mode);
}
