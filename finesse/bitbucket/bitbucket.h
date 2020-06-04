//
// (C) Copyright 2020 Tony Mason (fsgeek@cs.ubc.ca)
// All Rights Reserved
//

#if !defined(__BITBUCKET_H__)
#define __BITBUCKET_H__ (1)

#if !defined(FUSE_USE_VERSION)
#define FUSE_USE_VERSION 39
#endif // FUSE_USE_VERSION

#include <fuse_lowlevel.h>
#include <assert.h>
#include "bitbucketdata.h"

void bitbucket_init(void *userdata, struct fuse_conn_info *conn);
void bitbucket_destroy(void *userdata);
void bitbucket_lookup(fuse_req_t req, fuse_ino_t parent, const char *name);
void bitbucket_forget(fuse_req_t req, fuse_ino_t ino, uint64_t nlookup);
void bitbucket_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
void bitbucket_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr, int to_set, struct fuse_file_info *fi);
void bitbucket_readlink(fuse_req_t req, fuse_ino_t ino);
void bitbucket_mknod(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, dev_t rdev);
void bitbucket_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode);
void bitbucket_unlink(fuse_req_t req, fuse_ino_t parent, const char *name);
void bitbucket_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name);
void bitbucket_symlink(fuse_req_t req, const char *link, fuse_ino_t parent,	const char *name);
void bitbucket_rename(fuse_req_t req, fuse_ino_t parent, const char *name, fuse_ino_t newparent, const char *newname, unsigned int flags);
void bitbucket_link(fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent, const char *newname);
void bitbucket_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
void bitbucket_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);
void bitbucket_write(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t off, struct fuse_file_info *fi);
void bitbucket_flush(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
void bitbucket_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
void bitbucket_fsync(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi);
void bitbucket_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
void bitbucket_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);
void bitbucket_releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
void bitbucket_fsyncdir(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi);
void bitbucket_statfs(fuse_req_t req, fuse_ino_t ino);
void bitbucket_setxattr(fuse_req_t req, fuse_ino_t ino, const char *name, const char *value, size_t size, int flags);
void bitbucket_getxattr(fuse_req_t req, fuse_ino_t ino, const char *name, size_t size);
void bitbucket_listxattr(fuse_req_t req, fuse_ino_t ino, size_t size);
void bitbucket_removexattr(fuse_req_t req, fuse_ino_t ino, const char *name);
void bitbucket_access(fuse_req_t req, fuse_ino_t ino, int mask);
void bitbucket_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi);
void bitbucket_getlk(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, struct flock *lock);
void bitbucket_setlk(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, struct flock *lock, int sleep);
void bitbucket_bmap(fuse_req_t req, fuse_ino_t ino, size_t blocksize, uint64_t idx);
void bitbucket_ioctl(fuse_req_t req, fuse_ino_t ino, unsigned int cmd, void *arg, struct fuse_file_info *fi, unsigned flags, const void *in_buf, size_t in_bufsz, size_t out_bufsz);
void bitbucket_poll(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, struct fuse_pollhandle *ph);
void bitbucket_write_buf(fuse_req_t req, fuse_ino_t ino, struct fuse_bufvec *bufv, off_t off, struct fuse_file_info *fi);
void bitbucket_retrieve_reply(fuse_req_t req, void *cookie, fuse_ino_t ino, off_t offset, struct fuse_bufvec *bufv);
void bitbucket_forget_multi(fuse_req_t req, size_t count, struct fuse_forget_data *forgets);
void bitbucket_flock(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, int op);
void bitbucket_fallocate(fuse_req_t req, fuse_ino_t ino, int mode, off_t offset, off_t length, struct fuse_file_info *fi);
void bitbucket_readdirplus(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);
void bitbucket_copy_file_range(fuse_req_t req, fuse_ino_t ino_in, off_t off_in, struct fuse_file_info *fi_in, fuse_ino_t ino_out, off_t off_out, struct fuse_file_info *fi_out, size_t len, int flags);
void bitbucket_lseek(fuse_req_t req, fuse_ino_t ino, off_t off, int whence, struct fuse_file_info *fi);

bitbucket_dir_t *BitbucketCreateDirectory(bitbucket_dir_t *Parent, const char *DirName);


#endif // __BITBUCKET_H__
