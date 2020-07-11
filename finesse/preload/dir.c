/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

#include <finesse.h>
#include "preload.h"
#include <fcntl.h>           /* Definition of AT_* constants */
#include <unistd.h>

DIR *opendir(const char *name);
struct dirent *readdir(DIR *dir);
int closedir(DIR *dir);
void rewinddir(DIR *dir);

void todo_opendir(void);
void todo_opendir(void)
{

}
