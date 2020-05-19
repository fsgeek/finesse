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
ssize_t getxattr(const char *path, const char *name, void *value, size_t size);
ssize_t lgetxattr(const char *path, const char *name, void *value, size_t size);
ssize_t fgetxattr(int fd, const char *name, void *value, size_t size);
ssize_t listxattr(const char *path, char *list, size_t size);
ssize_t llistxattr(const char *path, char *list, size_t size);
ssize_t flistxattr(int fd, char *list, size_t size);
int flock(int fd, int operation)       
int getdents(unsigned int fd, struct linux_dirent *dirp, unsigned int count);
int getdents64(unsigned int fd, struct linux_dirent64 *dirp, unsigned int count);
int inotify_add_watch(int fd, const char *pathname, uint32_t mask);
int inotify_rm_watch(int fd, int wd);
int io_cancel(aio_context_t ctx_id, struct iocb *iocb, struct io_event *result);
int io_destroy(aio_context_t ctx_id);
int io_getevents(aio_context_t ctx_id, long min_nr, long nr, struct io_event *events, struct timespec *timeout);
int ioctl(int fd, unsigned long request, ...);
int chown(const char *pathname, uid_t owner, gid_t group);
int fchown(int fd, uid_t owner, gid_t group);
int lchown(const char *pathname, uid_t owner, gid_t group);
int fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags);
int link(const char *oldpath, const char *newpath);
int linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags);
int stat(const char *pathname, struct stat *statbuf);
int fstat(int fd, struct stat *statbuf);
int lstat(const char *pathname, struct stat *statbuf);
int fstatat(int dirfd, const char *pathname, struct stat *statbuf, int flags);
int mknod(const char *pathname, mode_t mode, dev_t dev);
int mknodat(int dirfd, const char *pathname, mode_t mode, dev_t dev);
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int name_to_handle_at(int dirfd, const char *pathname, struct file_handle *handle, int *mount_id, int flags);
int open_by_handle_at(int mount_fd, struct file_handle *handle, int flags);
int poll(struct pollfd *fds, nfds_t nfds, int timeout);
int ppoll(struct pollfd *fds, nfds_t nfds, const struct timespec *tmo_p, const sigset_t *sigmask);
ssize_t pread(int fd, void *buf, size_t count, off_t offset);
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);
ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
ssize_t writev(int fd, const struct iovec *iov, int iovcnt);
ssize_t preadv(int fd, const struct iovec *iov, int iovcnt, off_t offset);
ssize_t pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset);
ssize_t preadv2(int fd, const struct iovec *iov, int iovcnt, off_t offset, int flags);
ssize_t pwritev2(int fd, const struct iovec *iov, int iovcnt, off_t offset, int flags);
int quotactl(int cmd, const char *special, int id, caddr_t addr);
ssize_t readahead(int fd, off64_t offset, size_t count);
int readdir(unsigned int fd, struct old_linux_dirent *dirp, unsigned int count);
ssize_t readlink(const char *pathname, char *buf, size_t bufsiz);
ssize_t readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsiz);
int renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath);
int renameat2(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, unsigned int flags);
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
int pselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timespec *timeout, const sigset_t *sigmask);
int renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath);
int renameat2(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, unsigned int flags);
ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count);
ssize_t splice(int fd_in, loff_t *off_in, int fd_out, loff_t *off_out, size_t len, unsigned int flags);
int statx(int dirfd, const char *pathname, int flags, unsigned int mask, struct statx *statxbuf);
int symlink(const char *target, const char *linkpath);
int symlinkat(const char *target, int newdirfd, const char *linkpath);
int syncfs(int fd);
int truncate(const char *path, off_t length);
int ftruncate(int fd, off_t length);
int unlinkat(int dirfd, const char *pathname, int flags);
int ustat(dev_t dev, struct ustat *ubuf);


The entire POSIX aio package: http://man7.org/linux/man-pages/man7/aio.7.html
Note that this is a separate library.
#endif // 0
