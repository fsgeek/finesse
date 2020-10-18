/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

#include <finesse.h>
#include "preload.h"

int chdir(const char *pathname)
{
    return finesse_chdir(pathname);
}
