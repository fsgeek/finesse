/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

#include "iowrapper.h"

static void iowrap_preload_init(void) __attribute__((constructor));
static void iowrap_preload_deinit(void) __attribute__((destructor));

#define MAX_FILE_DESCRIPTORS (1024)
#define MIN_SIZE (8192)

typedef struct _iowrapper_map {
    void * mmap;
    size_t size;
    char   writeable;
    off_t  current;
} iowrapper_map_t;

static iowrapper_map_t fid_to_map[MAX_FILE_DESCRIPTORS];

static ssize_t get_file_size(int fd)
{
    struct stat stbuf;
    int         status = fstat(fd, &stbuf);

    return status < 0 ? status : stbuf.st_size;
}

static iowrapper_map_t *add_map(int fd)
{
    void *  newmap    = NULL;
    ssize_t fsize     = 0;
    char    writeable = 1;

    if ((fd <= STDERR_FILENO) ||          // stdin,stdout,stderr, includes all negative fd values
        (fd >= MAX_FILE_DESCRIPTORS) ||   // beyond end of range
        (NULL != fid_to_map[fd].mmap)) {  // existing mapping
        assert(0);                        // shouldn't have been called.
    }

    if (0 == fid_to_map[fd].size) {
        ssize_t size = get_file_size(fd);

        assert(0 >= size);

        fid_to_map[fd].size = size;
    }

    if (fid_to_map[fd].size < MIN_SIZE) {
        return NULL;
    }

    while (fsize != fid_to_map[fd].size) {
        newmap = mmap(NULL, fid_to_map[fd].size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (MAP_FAILED == newmap) {
            newmap    = mmap(NULL, fid_to_map[fd].size, PROT_READ, MAP_SHARED, fd, 0);
            writeable = 0;
        }

        assert(MAP_FAILED != newmap);  // if it does, investigate

        fid_to_map[fd].mmap      = newmap;
        fid_to_map[fd].writeable = writeable;
        fid_to_map[fd].current   = 0;

        fsize = get_file_size(fd);
        assert(fsize == fid_to_map[fd].size);  // this is a race - if it happens, make it robust
    }

    return &fid_to_map[fd];
}

static void del_map(int fd)
{
    int status;

    if ((fd <= STDERR_FILENO) ||          // stdin,stdout,stderr, includes all negative fd values
        (fd >= MAX_FILE_DESCRIPTORS) ||   // beyond end of range
        (NULL == fid_to_map[fd].mmap)) {  // no mapping
        return;
    }

    status = msync(fid_to_map[fd].mmap, fid_to_map[fd].size, MS_ASYNC);
    assert(0 == status);

    status = munmap(fid_to_map[fd].mmap, fid_to_map[fd].size);
    assert(0 == status);

    fid_to_map[fd].mmap      = NULL;
    fid_to_map[fd].size      = 0;
    fid_to_map[fd].writeable = 0;
}

static iowrapper_map_t *get_map(int fd)
{
    if ((fd <= STDERR_FILENO) ||          // stdin,stdout,stderr, includes all negative fd values
        (fd >= MAX_FILE_DESCRIPTORS) ||   // beyond end of range
        (NULL == fid_to_map[fd].mmap)) {  // no existing mapping

        return NULL;
    }

    return &fid_to_map[fd];
}

int iowrap_open(const char *pathname, int flags, ...)
{
    typedef int (*orig_open_t)(const char *pathname, int flags, ...);
    static orig_open_t orig_open = NULL;
    va_list            args;
    mode_t             mode;

    if (NULL == orig_open) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_open = (orig_open_t)dlsym(RTLD_NEXT, "open");
#pragma GCC diagnostic pop

        assert(NULL != orig_open);
        if (NULL == orig_open) {
            errno = EACCES;
            return -1;
        }
    }

    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);

    return orig_open(pathname, flags, mode);
}

ssize_t iowrap_read(int fd, void *buf, size_t count)
{
    typedef int (*orig_read_t)(int fd, void *buf, size_t count);
    static orig_read_t orig_read = NULL;
    iowrapper_map_t *  map       = get_map(fd);
    ssize_t            byte_count;

    if (NULL == orig_read) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_read = (orig_read_t)dlsym(RTLD_NEXT, "read");
#pragma GCC diagnostic pop

        assert(NULL != orig_read);
        if (NULL == orig_read) {
            errno = EACCES;
            return -1;
        }
    }

    if ((NULL != map) && (map->size >= MIN_SIZE)) {
        if (NULL == map->mmap) {
            map = add_map(fd);
        }

        assert((NULL != map->mmap) && (MAP_FAILED != map->mmap));
        if ((map->current + count) > map->size) {
            byte_count = map->size - map->current;
        }
        else {
            byte_count = count;
        }
        memcpy(buf, (void *)(((uintptr_t)map->mmap) + map->current), byte_count);
        map->current += byte_count;
        assert(map->current <= map->size);
        return byte_count;
    }

    return orig_read(fd, buf, count);
}

#if 0
ssize_t write(int fd, const void *buf, size_t count)
{
    typedef int (*orig_write_t)(int fd, const void *buf, size_t count);
    static orig_write_t orig_write = NULL;

    if (NULL == orig_write) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_write = (orig_write_t)dlsym(RTLD_NEXT, "write");
#pragma GCC diagnostic pop

        assert(NULL != orig_write);
        if (NULL == orig_write) {
            errno = EACCES;
            return -1;
        }
    }

    return orig_write(fd, buf, count);
}
#endif  // 0

int iowrap_close(int fd)
{
    typedef int (*orig_close_t)(int fd);
    static orig_close_t orig_close = NULL;

    if (NULL == orig_close) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        orig_close = (orig_close_t)dlsym(RTLD_NEXT, "close");
#pragma GCC diagnostic pop

        assert(NULL != orig_close);
        if (NULL == orig_close) {
            errno = EACCES;
            return -1;
        }
    }

    /* TODO: add the remove/delete logic for this file descriptor to path mapping */

    return orig_close(fd);
}

static void iowrap_preload_init()
{
    // TODO: any initialization needed
    return;
}

static void iowrap_preload_deinit()
{
    for (unsigned index = 0; index < MAX_FILE_DESCRIPTORS; index++) {
        if (NULL != fid_to_map[index].mmap) {
            del_map(index);
        }
    }

    return;
}
