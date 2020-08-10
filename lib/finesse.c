/*
  FINESSE: Low Energy effcient extension to FUSE
  Copyright (C) 2017-2020  Tony Mason <fsgeek@cs.ubc.ca>

*/

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE             /* See feature_test_macros(7) */
#endif // _GNU_SOURCE

#include "config.h"
#include "fuse_i.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include "fuse_kernel.h"
#include "fuse_opt.h"
#pragma GCC diagnostic pop
#include "fuse_misc.h"
#include "fuse_log.h"
#include <fuse_lowlevel.h>
#include "finesse-fuse.h"
#include <finesse-server.h>

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <sys/file.h>
#include <time.h>
#include <mqueue.h>
#include <uuid/uuid.h>

#if !defined(offset_of)
#define offset_of(type, field) (unsigned long)&(((type *)0)->field)
#endif // offset_of

#if !defined(container_of)
#define container_of(ptr, type, member) ((type *)(((char *)ptr) - offset_of(type, member)))
#endif // container_of



/* TODO: add remove! */
const struct fuse_lowlevel_ops *finesse_original_ops;

static int finesse_mt = 1;

static void finesse_fuse_init(void *userdata, struct fuse_conn_info *conn)
{
    assert(NULL != finesse_original_ops->init);
    finesse_original_ops->init(userdata, conn);
}

#define FINESSE_CHECK_ORIGINAL_OP(req, original_op) \
do { \
    if (NULL == finesse_original_ops->original_op) { \
        fuse_reply_err(req, ENOSYS); \
    } \
} while(0)

static void finesse_destroy(void *userdata)
{
    if (finesse_original_ops->destroy) {
        finesse_original_ops->destroy(userdata);
    }

}


static void finesse_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
    FINESSE_CHECK_ORIGINAL_OP(req, lookup);

    finesse_set_provider(req, 0);
    req->finesse.notify = 1;
    finesse_original_ops->lookup(req, parent, name);
}

static void finesse_forget(fuse_req_t req, fuse_ino_t ino, uint64_t nlookup)
{
    FINESSE_CHECK_ORIGINAL_OP(req, forget);

    finesse_set_provider(req, 0);
    req->finesse.notify = 1;
    /* TODO: remove from lookup table? */
    finesse_original_ops->forget(req, ino, nlookup);
}

static void finesse_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    FINESSE_CHECK_ORIGINAL_OP(req, getattr);

    finesse_set_provider(req, 0);
    finesse_original_ops->getattr(req, ino, fi);
}


static void finesse_setattr(fuse_req_t req, fuse_ino_t nodeid, struct stat *attr, int to_set, struct fuse_file_info *fi)
{
    FINESSE_CHECK_ORIGINAL_OP(req, setattr);

    finesse_set_provider(req, 0);
    finesse_original_ops->setattr(req, nodeid, attr, to_set, fi);
}

static void finesse_readlink(fuse_req_t req, fuse_ino_t ino)
{
    FINESSE_CHECK_ORIGINAL_OP(req, readlink);

    finesse_set_provider(req, 0);
    finesse_original_ops->readlink(req, ino);
}

static void finesse_mknod(fuse_req_t req, fuse_ino_t parent, const char *name,
				  mode_t mode, dev_t rdev)
{
    FINESSE_CHECK_ORIGINAL_OP(req, mknod);

    finesse_set_provider(req, 0);
    finesse_original_ops->mknod(req, parent, name, mode, rdev);

}

// name is to avoid a collision with the internal implemenation - bleh.
static void finesse_makedir(fuse_req_t req, fuse_ino_t nodeid, const char *name, mode_t mode)
{
    FINESSE_CHECK_ORIGINAL_OP(req, mkdir);

    finesse_set_provider(req, 0);
    finesse_original_ops->mkdir(req, nodeid, name, mode);
}

// nonstandard name to avoid the internal implementation - bleh.
static void finesse_fuse_unlink(fuse_req_t req, fuse_ino_t parent, const char *name)
{
    FINESSE_CHECK_ORIGINAL_OP(req, unlink);

    finesse_set_provider(req, 0);
    finesse_original_ops->unlink(req, parent, name);
}


static void finesse_rmdir(fuse_req_t req, fuse_ino_t parent, const char* name)
{
    FINESSE_CHECK_ORIGINAL_OP(req, rmdir);

    finesse_set_provider(req, 0);
    finesse_original_ops->rmdir(req, parent, name);
}

static void finesse_symlink(fuse_req_t req, const char *link, fuse_ino_t parent,
					const char *name)
{
    FINESSE_CHECK_ORIGINAL_OP(req, symlink);

    finesse_set_provider(req, 0);
    finesse_original_ops->symlink(req, link, parent, name);
}

static void finesse_rename(fuse_req_t req, fuse_ino_t parent, const char *name,
                fuse_ino_t newparent, const char *newname,
                unsigned int flags)
{
    FINESSE_CHECK_ORIGINAL_OP(req, rename);

    finesse_set_provider(req, 0);
    finesse_original_ops->rename(req, parent, name, newparent, newname, flags);

}

static void finesse_link(fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent,
				 const char *newname)
{
    FINESSE_CHECK_ORIGINAL_OP(req, link);

    finesse_set_provider(req, 0);
    finesse_original_ops->link(req, ino, newparent, newname);
}

static void finesse_fuse_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    FINESSE_CHECK_ORIGINAL_OP(req, open);

    finesse_set_provider(req, 0);
    finesse_original_ops->open(req, ino, fi);
}

static void finesse_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi)
{
    FINESSE_CHECK_ORIGINAL_OP(req, read);

    finesse_set_provider(req, 0);
    finesse_original_ops->read(req, ino, size, offset, fi);
}

static void finesse_write(fuse_req_t req, fuse_ino_t nodeid, const char * buf,
                          size_t size, off_t off, struct fuse_file_info *fi)
{
    FINESSE_CHECK_ORIGINAL_OP(req, rmdir);

    finesse_set_provider(req, 0);
    req->finesse.notify = 1;
    finesse_original_ops->write(req, nodeid, buf, size, off, fi);
}

static void finesse_flush(fuse_req_t req, fuse_ino_t nodeid, struct fuse_file_info *fi)
{
    FINESSE_CHECK_ORIGINAL_OP(req, flush);

    finesse_set_provider(req, 0);
    finesse_original_ops->flush(req, nodeid, fi);
}


static void finesse_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    finesse_object_t *finobj = finesse_object_lookup_by_ino(ino);

    if (NULL != finobj)
    {
        /* basically, this is saying that this is no longer in use, so we remove the lookup reference */
        finesse_object_release(finobj);
    }

    FINESSE_CHECK_ORIGINAL_OP(req, release);

    finesse_set_provider(req, 0);
    req->finesse.notify = 1;
    finesse_original_ops->release(req, ino, fi);
}

static void finesse_fsync(fuse_req_t req, fuse_ino_t nodeid, int datasync, struct fuse_file_info *fi)
{
    FINESSE_CHECK_ORIGINAL_OP(req, fsync);

    finesse_set_provider(req, 0);
    finesse_original_ops->fsync(req, nodeid, datasync, fi);
}

static void finesse_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    FINESSE_CHECK_ORIGINAL_OP(req, opendir);

    finesse_set_provider(req, 0);
    req->finesse.notify = 1;
    finesse_original_ops->opendir(req, ino, fi);
}


static void finesse_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi)
{
    FINESSE_CHECK_ORIGINAL_OP(req, readdir);

    finesse_set_provider(req, 0);
    finesse_original_ops->readdir(req, ino, size, offset, fi);
}

static void finesse_releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    FINESSE_CHECK_ORIGINAL_OP(req, releasedir);

    finesse_set_provider(req, 0);
    req->finesse.notify = 1;
    finesse_original_ops->releasedir(req, ino, fi);
}

static void finesse_fsyncdir(fuse_req_t req, fuse_ino_t ino, int datasync,
					 struct fuse_file_info *fi)
{
    FINESSE_CHECK_ORIGINAL_OP(req, fsyncdir);

    finesse_set_provider(req, 0);
    req->finesse.notify = 0;
    finesse_original_ops->fsyncdir(req, ino, datasync, fi);
}


static void finesse_fuse_statfs(fuse_req_t req, fuse_ino_t nodeid)
{
    FINESSE_CHECK_ORIGINAL_OP(req, statfs);

    finesse_set_provider(req, 0);
    req->finesse.notify = 1;
    finesse_original_ops->statfs(req, nodeid);
}

static void finesse_setxattr(fuse_req_t req, fuse_ino_t ino, const char *name,
					 const char *value, size_t size, int flags)
{
    FINESSE_CHECK_ORIGINAL_OP(req, setxattr);

    finesse_set_provider(req, 0);
    req->finesse.notify = 0;
    finesse_original_ops->setxattr(req, ino, name, value, size, flags);
}


static void finesse_getxattr(fuse_req_t req, fuse_ino_t nodeid, const char *name, size_t size)
{
    FINESSE_CHECK_ORIGINAL_OP(req, getxattr);

    finesse_set_provider(req, 0);
    finesse_original_ops->getxattr(req, nodeid, name, size);
}

static void finesse_listxattr(fuse_req_t req, fuse_ino_t ino, size_t size)
{
    FINESSE_CHECK_ORIGINAL_OP(req, listxattr);

    finesse_set_provider(req, 0);
    finesse_original_ops->listxattr(req, ino, size);
}

static void finesse_removexattr(fuse_req_t req, fuse_ino_t ino, const char *name)
{
    FINESSE_CHECK_ORIGINAL_OP(req, removexattr);

    finesse_set_provider(req, 0);
    finesse_original_ops->removexattr(req, ino, name);
}

// another name collision...
static void finesse_chkaccess(fuse_req_t req, fuse_ino_t ino, int mask)
{
    FINESSE_CHECK_ORIGINAL_OP(req, access);

    finesse_set_provider(req, 0);
    finesse_original_ops->access(req, ino, mask);
}

static void finesse_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi)
{
    FINESSE_CHECK_ORIGINAL_OP(req, create);

    finesse_set_provider(req, 0);
    finesse_original_ops->create(req, parent, name, mode, fi);
}

static void finesse_getlk(fuse_req_t req, fuse_ino_t ino,
				  struct fuse_file_info *fi, struct flock *lock)
{
    FINESSE_CHECK_ORIGINAL_OP(req, getlk);

    finesse_set_provider(req, 0);
    finesse_original_ops->getlk(req, ino, fi, lock);
}

static void finesse_setlk(fuse_req_t req, fuse_ino_t ino,
				  struct fuse_file_info *fi,
				  struct flock *lock, int sleep)
{
    FINESSE_CHECK_ORIGINAL_OP(req, setlk);
    finesse_set_provider(req, 0);
    finesse_original_ops->setlk(req, ino, fi, lock, sleep);
}

static void finesse_bmap(fuse_req_t req, fuse_ino_t ino, size_t blocksize,
				 uint64_t idx)
{
    FINESSE_CHECK_ORIGINAL_OP(req, bmap);
    finesse_set_provider(req, 0);
    finesse_original_ops->bmap(req, ino, blocksize, idx);
}

static void finesse_ioctl(fuse_req_t req, fuse_ino_t ino, unsigned int cmd,
		       void *arg, struct fuse_file_info *fi, unsigned flags,
		       const void *in_buf, size_t in_bufsz, size_t out_bufsz)
{
    FINESSE_CHECK_ORIGINAL_OP(req, ioctl);
    finesse_set_provider(req, 0);
    finesse_original_ops->ioctl(req, ino, cmd, arg, fi, flags, in_buf, in_bufsz, out_bufsz);
}

static void finesse_poll(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi,
				 struct fuse_pollhandle *ph)
{
    FINESSE_CHECK_ORIGINAL_OP(req, poll);
    finesse_set_provider(req, 0);
    finesse_original_ops->poll(req, ino, fi, ph);
}

static void finesse_write_buf(fuse_req_t req, fuse_ino_t ino, struct fuse_bufvec *in_buf, off_t off, struct fuse_file_info *fi)
{
    FINESSE_CHECK_ORIGINAL_OP(req, write_buf);

    finesse_set_provider(req, 0);
    finesse_original_ops->write_buf(req, ino, in_buf, off, fi);
}

static void finesse_retrieve_reply(fuse_req_t req, void *cookie, fuse_ino_t ino,
						   off_t offset, struct fuse_bufvec *bufv)
{
    FINESSE_CHECK_ORIGINAL_OP(req, retrieve_reply);
    finesse_set_provider(req, 0);
    finesse_original_ops->retrieve_reply(req, cookie, ino, offset, bufv);
}


static void finesse_forget_multi(fuse_req_t req, size_t count, struct fuse_forget_data *forgets)
{
    FINESSE_CHECK_ORIGINAL_OP(req, forget_multi);

    finesse_set_provider(req, 0);
    finesse_original_ops->forget_multi(req, count, forgets);
}

static void finesse_flock(fuse_req_t req, fuse_ino_t ino,
				  struct fuse_file_info *fi, int op)
{
    FINESSE_CHECK_ORIGINAL_OP(req, flock);
    finesse_set_provider(req, 0);
    finesse_original_ops->flock(req, ino, fi, op);
}

static void finesse_fallocate(fuse_req_t req, fuse_ino_t ino, int mode,
                    off_t offset, off_t length, struct fuse_file_info *fi)
{
    FINESSE_CHECK_ORIGINAL_OP(req, fallocate);
    finesse_set_provider(req, 0);
    finesse_original_ops->fallocate(req, ino, mode, offset, length, fi);
}

static void finesse_readdirplus(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi)
{
    FINESSE_CHECK_ORIGINAL_OP(req, readdirplus);

    finesse_set_provider(req, 0);
    finesse_original_ops->readdirplus(req, ino, size, offset, fi);
}

static void finesse_copy_file_range(fuse_req_t req, fuse_ino_t ino_in,
				 off_t off_in, struct fuse_file_info *fi_in,
				 fuse_ino_t ino_out, off_t off_out,
				 struct fuse_file_info *fi_out, size_t len,
				 int flags)
{
    FINESSE_CHECK_ORIGINAL_OP(req, copy_file_range);
    finesse_set_provider(req, 0);
    finesse_original_ops->copy_file_range(req, ino_in, off_in, fi_in, ino_out, off_out, fi_out, len, flags);

}

static void finesse_lseek(fuse_req_t req, fuse_ino_t ino, off_t off, int whence,
		       struct fuse_file_info *fi)
{
    FINESSE_CHECK_ORIGINAL_OP(req, lseek);
    finesse_set_provider(req, 0);
    finesse_original_ops->lseek(req, ino, off, whence, fi);
}


static struct fuse_lowlevel_ops finesse_ops = {
    .init            = finesse_fuse_init,
    .destroy         = finesse_destroy,
    .lookup          = finesse_lookup,
    .forget          = finesse_forget,
    .getattr         = finesse_getattr,
    .setattr         = finesse_setattr,
    .readlink        = finesse_readlink,
    .mknod           = finesse_mknod,
    .mkdir           = finesse_makedir,
    .unlink          = finesse_fuse_unlink,
    .rmdir           = finesse_rmdir,
    .symlink         = finesse_symlink,
    .rename          = finesse_rename,
    .link            = finesse_link,
    .open            = finesse_fuse_open,
    .read            = finesse_read,
    .write           = finesse_write,
    .flush           = finesse_flush,
    .release         = finesse_release,
    .fsync           = finesse_fsync,
    .opendir         = finesse_opendir,
    .readdir         = finesse_readdir,
    .releasedir      = finesse_releasedir,
    .fsyncdir        = finesse_fsyncdir,
    .statfs          = finesse_fuse_statfs,
    .setxattr        = finesse_setxattr,
    .getxattr        = finesse_getxattr,
    .listxattr       = finesse_listxattr,
    .removexattr     = finesse_removexattr,
    .access          = finesse_chkaccess,
    .create          = finesse_create,
    .getlk           = finesse_getlk,
    .setlk           = finesse_setlk,
    .bmap            = finesse_bmap,
    .ioctl           = finesse_ioctl,
    .poll            = finesse_poll,
    .write_buf       = finesse_write_buf,
    .retrieve_reply  = finesse_retrieve_reply,
    .forget_multi    = finesse_forget_multi,
    .flock           = finesse_flock,
    .fallocate       = finesse_fallocate,
    .readdirplus     = finesse_readdirplus,
    .copy_file_range = finesse_copy_file_range,
    .lseek           = finesse_lseek,
    };

uuid_t finesse_server_uuid;

/**
 * Set the provider for the given request.
 */
void finesse_set_provider(fuse_req_t req, int finesse)
{
    if (finesse)
    {
        req->finesse.allocated = 1;
    }
    else
    {
        req->finesse.allocated = 0;
    }
}

int finesse_get_provider(fuse_req_t req)
{
    return req->finesse.allocated;
}

#undef fuse_session_new

struct fuse_session *finesse_session_new(struct fuse_args *args,
                                         const struct fuse_lowlevel_ops *op,
                                         size_t op_size, void *userdata)
{
    (void)op;
    struct fuse_session *se;

    //
    // Save the original ops
    //
    finesse_original_ops = op;

    se = fuse_session_new(args, &finesse_ops, op_size, userdata);

    if (NULL == se)
    {
        return se;
    }

    return se;
}

void finesse_session_mount(struct fuse_session *se);

void finesse_session_mount(struct fuse_session *se)
{
    if (NULL != se->server_handle) {
        // already done?
        return;
    }

    if (NULL == se->mountpoint) {
        fuse_log(FUSE_LOG_ERR, "FINESSE: no mountpoint, cannot create connection\n");
        return;
    }

    if (0 > FinesseStartServerConnection(se->mountpoint, &se->server_handle))
    {
        fuse_log(FUSE_LOG_ERR, "FINESSE: failed to start Finesse Server connection\n");
        se->server_handle = NULL;
    }

    fuse_log(FUSE_LOG_INFO, "FINESSE: started Finesse Server connection\n");

    return;
}

static const char *finesse_get_string_for_message_type(FINESSE_MESSAGE_TYPE Type)
{
    const char *str = "UNKNOWN MESSAGE TYPE";

    switch(Type) {
        case FINESSE_REQUEST:
            str = "FINESSE REQUEST";
            break;
        case FINESSE_RESPONSE:
            str = "FINESSE RESPONSE";
            break;
        default:
            break; // use default string
    }

    return str;
}

static const char *finesse_get_string_for_message_class(FINESSE_MESSAGE_CLASS Class)
{
    const char *str = "UNKNOWN MESSAGE CLASS";

    switch(Class) {
        case FINESSE_FUSE_MESSAGE:
            str = "FUSE MESSAGE CLASS";
            break;
        case FINESSE_NATIVE_MESSAGE:
            str = "NATIVE MESSAGE CLASS";
            break;
        default:
            break; // use default string
    }

    return str;
}


static void *finesse_process_request_worker(void *arg)
{
    struct fuse_session *se = (struct fuse_session *)arg;
    finesse_server_handle_t fsh = (finesse_server_handle_t)se->server_handle;

    while (fsh)
    {
        int status;
        void *client;
        fincomm_message request;
        finesse_msg *fmsg = NULL;

        status = FinesseGetRequest(fsh, &client, &request);
        assert(0 == status);
        assert(NULL != request);
        assert((uintptr_t)client < SHM_MESSAGE_COUNT);
        assert(0 != request->RequestId); // invalid request number

        assert(FINESSE_REQUEST == request->MessageType); // nothing else makes sense here
        fmsg = (finesse_msg *) request->Data;
        assert(NULL != fmsg);
        fuse_log(FUSE_LOG_DEBUG, "FINESSE: message 0x%p type %s class %s\n",
                 fmsg, finesse_get_string_for_message_type(request->MessageType),
                 finesse_get_string_for_message_class(fmsg->MessageClass));

        status = EINVAL;
        switch (fmsg->MessageClass) {
            default: {
                // Bad request
                request->Result = EINVAL;
                request->MessageType = FINESSE_RESPONSE;
                status = FinesseSendResponse(se->server_handle, client, request);
                assert(0 == status);
            }
            break;

            case FINESSE_FUSE_MESSAGE: {
                status = FinesseServerHandleFuseRequest(se, client, request);
            }
            break;

            case FINESSE_NATIVE_MESSAGE: {
                status = FinesseServerHandleNativeRequest(se, client, request);
            }
            break;
        }
        assert(0 == status); // shouldn't be failing
    }

    if (NULL != fsh) {
        FinesseStopServerConnection(fsh);
        fsh = NULL;
    }

    return NULL;
}

void finesse_notify_reply_iov(fuse_req_t req, int error, struct iovec *iov, int count)
{
    (void)iov;

    if (0 != error)
    {
        // So far we don't care about the error outcomes
        return;
    }

    if (count < 2)
    {
        // not sure what this means
        return;
    }

    //
    // We want to process some requests here
    //
    switch (req->opcode)
    {
        default:
            break;
        case FUSE_LOOKUP:
        {
            ino_t ino;
            struct fuse_entry_out *arg = (struct fuse_entry_out *)iov[1].iov_base;
            finesse_object_t *nicobj;
            uuid_t uuid;

            uuid_generate_time_safe(uuid);

            assert(iov[1].iov_len >= sizeof(struct fuse_entry_out));
            ino = arg->nodeid;
            nicobj = finesse_object_create(ino, &uuid);
            assert(NULL != nicobj);
            finesse_object_release(nicobj);
        }
    }

}

int finesse_send_reply_iov(fuse_req_t req, int error, struct iovec *iov, int count, int free_req)
{
    struct finesse_req *freq = NULL;

    // Note: we'll probably have to add additional case handling here (right now this is just lookup)
    // TODO: should we be DOING something with error?

    if (error <= -1000 || error > 0)
    {
        fprintf(stderr, "fuse: bad error value: %i\n", error);
        // error = -ERANGE;
        assert(0); // this situation isn't handled presently.
    }

    assert(NULL != req);
    assert(NULL != req->se);
    assert(req->finesse.allocated); // otherwise, shouldn't be here
    freq = (struct finesse_req *)req;
    assert(NULL == freq->iov); // if not, we've got to clean up what IS there - but why would this happen?
    freq->iov = (struct iovec *)malloc(count * sizeof(struct iovec));
    assert(NULL != freq->iov);
    freq->iov_count = count;

    // capture all the io vector data
    for (unsigned index = 0; index < count; index++) {
        freq->iov[index].iov_base = malloc(iov[index].iov_len);
        assert(NULL != freq->iov[index].iov_base);
        memcpy(freq->iov[index].iov_base, iov[index].iov_base, iov[index].iov_len);
        freq->iov[index].iov_len = iov[index].iov_len;
    }

    // signal the waiter
    FinesseSignalFuseRequestCompletion(freq);

    assert(0 == free_req); // we don't handle the "don't free" case at this point.

    return 0;
}

// static struct sigevent finesse_mq_sigevent;
static pthread_attr_t finesse_mq_thread_attr;
#define FINESSE_MAX_THREADS (1)
pthread_t finesse_threads[FINESSE_MAX_THREADS];
#undef fuse_session_loop_mt

int finesse_session_loop_mt(struct fuse_session *se, struct fuse_loop_config *config)
{
    int status;

    finesse_mt = 1;

    status = 0;

    while (NULL != se->server_handle)
    {
        memset(&finesse_mq_thread_attr, 0, sizeof(finesse_mq_thread_attr));
        status = pthread_attr_init(&finesse_mq_thread_attr);
        if (status < 0)
        {
            fprintf(stderr, "finesse (fuse): pthread_attr_init failed: %s\n", strerror(errno));
            return status; // no cleanup
        }
        status = pthread_attr_setdetachstate(&finesse_mq_thread_attr, PTHREAD_CREATE_DETACHED);
        if (status < 0)
        {
            fprintf(stderr, "finesse (fuse): pthread_attr_setdetachstate failed: %s\n", strerror(errno));
            break;
        }

        uuid_generate_time_safe(finesse_server_uuid);

        /* TODO: start worker thread(s) */
        for (unsigned int index = 0; index < FINESSE_MAX_THREADS; index++)
        {
            status = pthread_create(&finesse_threads[index], &finesse_mq_thread_attr, finesse_process_request_worker, se);
            if (status < 0)
            {
                fprintf(stderr, "finesse (fuse): pthread_create failed: %s\n", strerror(errno));
            }
        }

        /* done */
        break;
    }

    if (status < 0)
    {
        pthread_attr_destroy(&finesse_mq_thread_attr);
        return status;
    }

    return fuse_session_loop_mt(se, config);
}

int finesse_session_loop_mt_31(struct fuse_session *se, int clone_fd)
{
    struct fuse_loop_config config;
    config.clone_fd = clone_fd;
    config.max_idle_threads = 10;
    return finesse_session_loop_mt(se, &config);
}

int finesse_session_loop(struct fuse_session *se)
{
    /* for now we don't support any finesse functionality in single threaded mode */
    finesse_mt = 0;
    return fuse_session_loop(se);
}

void finesse_session_destroy(struct fuse_session *se)
{
    /* TODO: need to add the finesse specific logic here */

    if (NULL != se->server_handle)
    {
        FinesseStopServerConnection(se->server_handle);
        se->server_handle = NULL;
    }

    fuse_session_destroy(se);
}
