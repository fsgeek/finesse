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

int __fxstat(int __ver, int __fildes, struct stat *__stat_buf)
{
    if (_STAT_VER != __ver) {
        fprintf(stderr, "%s:%d call fails, ver = %d, expects %d\n", __func__, __LINE__, __ver, _STAT_VER);
        errno = EINVAL;
        return -1;
    }

    return finesse_fstat(__fildes, __stat_buf);
}
int __xstat(int __ver, const char *__filename, struct stat *__stat_buf)
{
    int status;

    if (_STAT_VER != __ver) {
        fprintf(stderr, "%s:%d call fails, ver = %d, expects %d\n", __func__, __LINE__, __ver, _STAT_VER);
        errno = EINVAL;
        return -1;
    }

    status = finesse_stat(__filename, __stat_buf);

#if 0
    if (0 != status) {
        fprintf(stderr, "\npreload: __xstat(\"%s\"), status = %d, errno = %d\n", __filename, status, errno);
    }
#endif  // 0

    return status;
}

int __lxstat(int __ver, const char *__filename, struct stat *__stat_buf)
{
    if (_STAT_VER != __ver) {
        fprintf(stderr, "%s:%d call fails, ver = %d, expects %d\n", __func__, __LINE__, __ver, _STAT_VER);
        errno = EINVAL;
        return -1;
    }

    return finesse_lstat(__filename, __stat_buf);
}

int __fxstatat(int __ver, int __fildes, const char *__filename, struct stat *__stat_buf, int __flag)
{
    if (_STAT_VER != __ver) {
        fprintf(stderr, "%s:%d call fails, ver = %d, expects %d\n", __func__, __LINE__, __ver, _STAT_VER);
        errno = EINVAL;
        return -1;
    }

    return finesse_fstatat(__fildes, __filename, __stat_buf, __flag);
}
