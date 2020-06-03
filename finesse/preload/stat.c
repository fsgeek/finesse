#include <finesse.h>
#include "preload.h"


int stat(const char *pathname, struct stat *statbuf)
{
    return finesse_stat(pathname, statbuf);
}

int fstat(int fd, struct stat *statbuf)
{
    return finesse_fstat(fd, statbuf);
}

int lstat(const char *pathname, struct stat *statbuf)
{
    return finesse_lstat(pathname, statbuf);
}

int fstatat(int dirfd, const char *pathname, struct stat *statbuf, int flags)
{
    return finesse_fstatat(dirfd, pathname, statbuf, flags);
}

