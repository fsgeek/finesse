/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

#include <finesse.h>
#include "preload.h"

int access(const char *pathname, int mode)
{
  return finesse_access(pathconf, mode);
}

int faccessat(int dirfd, const char *pathname, int mode, int flags)
{
  return finesse_faccessat(dirfd, pathname, mode, flags);
}
