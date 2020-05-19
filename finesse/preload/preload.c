/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

#include <finesse.h>
#include <unistd.h>

// File based APIs
//
// 
#if 0
char *getcwd(char *buf, size_t size);
int mkdir(const char *pathname, mode_t mode);
int rmdir(const char *pathname);
int chdir(const char *path);
int link(const char *oldpath, const char *newpath);
int unlink(const char *pathname);
int rename(const char *oldpath, const char *newpath);
int stat(const char *file_name, struct stat *buf);
int chmod(const char *path, mode_t mode);
int chown(const char *path, uid_t owner, gid_t group);
int utime(const char *filename, struct utimbuf *buf);
DIR *opendir(const char *name);
struct dirent *readdir(DIR *dir);
int closedir(DIR *dir);
void rewinddir(DIR *dir);
int access(const char *pathname, int mode);
int open(const char *pathname, int flags); - open.c
int creat(const char *pathname, mode_t mode);
int close(int fd);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int fcntl(int fd, int cmd);
int fstat(int filedes, struct stat *buf);
off_t lseek(int fildes, off_t offset, int whence);
int dup(int oldfd);
int dup2(int oldfd, int newfd);
int pipe(int filedes[2]);
int mkfifo ( const char *pathname, mode_t mode );
mode_t umask(mode_t mask);
#endif // 0

#if 0
// other calls we may want to consider
// http://man7.org/linux/man-pages/man2/syscalls.2.html
int posix_fadvise(int fd, off_t offset, off_t len, int advice);
int fallocate(int fd, int mode, off_t offset, off_t len);
int posix_fallocate(int fd, off_t offset, off_t len);
int fsync(int fd);
int fdatasync(int fd);
ssize_t getxattr(const char *path, const char *name,
                void *value, size_t size);
ssize_t lgetxattr(const char *path, const char *name,
                void *value, size_t size);
ssize_t fgetxattr(int fd, const char *name,
                void *value, size_t size);
ssize_t listxattr(const char *path, char *list, size_t size);
ssize_t llistxattr(const char *path, char *list, size_t size);
ssize_t flistxattr(int fd, char *list, size_t size);
int flock(int fd, int operation)       
int getdents(unsigned int fd, struct linux_dirent *dirp,
                unsigned int count);
int getdents64(unsigned int fd, struct linux_dirent64 *dirp,
                unsigned int count);
int inotify_add_watch(int fd, const char *pathname, uint32_t mask);
int inotify_rm_watch(int fd, int wd);
int io_cancel(aio_context_t ctx_id, struct iocb *iocb,
                struct io_event *result);
int io_destroy(aio_context_t ctx_id);
int io_getevents(aio_context_t ctx_id, long min_nr, long nr,
                        struct io_event *events, struct timespec *timeout);
#endif // 0
