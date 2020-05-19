/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

#include <finesse.h>
#include "preload.h"

int close(int fd)
{
    finesse_preload_init();

    return finesse_close(fd);
}