/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

#include <finesse.h>
#include "preload.h"
#include <fcntl.h>           /* Definition of AT_* constants */
#include <unistd.h>

int link(const char *oldpath, const char *newpath);

#include <fcntl.h>           /* Definition of AT_* constants */
#include <unistd.h>

int linkat(int olddirfd, const char *oldpath,
            int newdirfd, const char *newpath, int flags);

void todo_link(void);
void todo_link(void) 
{

}