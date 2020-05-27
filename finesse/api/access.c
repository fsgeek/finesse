/*
 * (C) Copyright 2017-2020 Tony Mason
 * All Rights Reserved
 */


#include "api-internal.h"

static int fin_access(const char *pathname, int mode)
{
    typedef int (*orig_access_t)(const char *path, int mode);
    static orig_access_t orig_access = NULL;

    if (NULL == orig_access) {
        orig_access = (orig_access_t)dlsym(RTLD_NEXT, "access");

        assert(NULL != orig_access);
        if (NULL == orig_access) {
            return EACCES;
        }
    }

    return orig_access(pathname, mode);

}


int finesse_access(const char *pathname, int mode)
{
    return fin_access(pathname, mode);
}

static int fin_faccessat(int dirfd, const char *pathname, int mode, int flags)
{
    typedef int (*orig_faccessat_t)(int dirfd, const char *pathname, int mode, int flags);
    static orig_faccessat_t orig_faccessat = NULL;

    if (NULL == orig_faccessat) {
        orig_faccessat = (orig_faccessat_t)dlsym(RTLD_NEXT, "faccessat");

        assert(NULL != orig_faccessat);
        if (NULL == orig_faccessat) {
            return EACCES;
        }
    }

    return orig_faccessat(dirfd, pathname, mode, flags);

}

int finesse_faccessat(int dirfd, const char *pathname, int mode, int flags)
{
    return fin_faccessat(dirfd, pathname, mode, flags);
}
