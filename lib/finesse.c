/*
  FINESSE: Low Energy effcient extension to FUSE
  Copyright (C) 2017  Tony Mason <fsgeek@cs.ubc.ca>

*/

#define _GNU_SOURCE

#include "config.h"
#include "fuse_i.h"
#include "fuse_kernel.h"
#include "fuse_opt.h"
#include "fuse_misc.h"
#include <fuse_lowlevel.h>
#include "finesse-fuse.h"
#include "finesse-lookup.h"
#include "finesse-list.h"
#include "finesse.h"
#include "finesse.pb-c.h"

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



typedef struct finesse_lookup_info
{
    int status;
    /* used so the caller can wait for completion */
    pthread_mutex_t lock;
    pthread_cond_t condition;

    /* attributes */
    struct fuse_attr attr;

} finesse_lookup_info_t;

/* TODO: add remove! */

struct finesse_req
{
    struct fuse_req fuse_request;

    /* finesse specific routing information */
};

static void list_init_req(struct fuse_req *req)
{
    req->next = req;
    req->prev = req;
}

static void list_del_req(struct fuse_req *req)
{
    assert(0);
    struct fuse_req *prev = req->prev;
    struct fuse_req *next = req->next;
    prev->next = next;
    next->prev = prev;
}

static struct fuse_req *finesse_alloc_req(struct fuse_session *se)
{
    struct fuse_req *req;

    req = (struct fuse_req *)calloc(1, sizeof(struct fuse_req));
    if (req == NULL)
    {
        fprintf(stderr, "finesse (fuse): failed to allocate request\n");
    }
    else
    {
        req->se = se;
        req->ctr = 1;
        list_init_req(req);
        fuse_mutex_init(&req->lock);
        finesse_set_provider(req, 1);
    }
    return req;
}

static void destroy_req(fuse_req_t req)
{
    pthread_mutex_destroy(&req->lock);
    free(req);
}

static void finesse_free_req(fuse_req_t req)
{
    int ctr;
    struct fuse_session *se = req->se;

    pthread_mutex_lock(&se->lock);
    req->u.ni.func = NULL;
    req->u.ni.data = NULL;
    list_del_req(req);
    ctr = --req->ctr;
    fuse_chan_put(req->ch);
    req->ch = NULL;
    pthread_mutex_unlock(&se->lock);
    if (!ctr)
        destroy_req(req);
}

static const struct fuse_lowlevel_ops *finesse_original_ops;

static int finesse_mt = 1;

static void finesse_fuse_init(void *userdata, struct fuse_conn_info *conn);
static void finesse_fuse_init(void *userdata, struct fuse_conn_info *conn)
{
    return finesse_original_ops->init(userdata, conn);
}

static void finesse_lookup(fuse_req_t req, fuse_ino_t parent, const char *name);
static void finesse_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
    finesse_set_provider(req, 0);
    req->finesse.notify = 1;
    /* TODO: add to lookup table? */
    return finesse_original_ops->lookup(req, parent, name);
}

static void finesse_mkdir(fuse_req_t req, fuse_ino_t nodeid, const char *name, mode_t mode) {
   finesse_set_provider(req, 0);
   return finesse_original_ops->mkdir(req, nodeid, name, mode);
}

static void finesse_forget(fuse_req_t req, fuse_ino_t ino, uint64_t nlookup);
static void finesse_forget(fuse_req_t req, fuse_ino_t ino, uint64_t nlookup)
{
    finesse_set_provider(req, 0);
    req->finesse.notify = 1;
    /* TODO: remove from lookup table? */
    return finesse_original_ops->forget(req, ino, nlookup);
}

static void finesse_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
static void finesse_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    finesse_set_provider(req, 0);
    return finesse_original_ops->getattr(req, ino, fi);
}

static void finesse_readlink(fuse_req_t req, fuse_ino_t ino);
static void finesse_readlink(fuse_req_t req, fuse_ino_t ino)
{
    finesse_set_provider(req, 0);
    return finesse_original_ops->readlink(req, ino);
}

static void finesse_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
static void finesse_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    finesse_set_provider(req, 0);
    req->finesse.notify = 1;
    return finesse_original_ops->opendir(req, ino, fi);
}

static void finesse_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi);
static void finesse_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi)
{
    finesse_set_provider(req, 0);
    return finesse_original_ops->readdir(req, ino, size, offset, fi);
}

//static void finesse_readdirplus(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi);
//static void finesse_readdirplus(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi)
//{
//    finesse_set_provider(req, 0);
//    return finesse_original_ops->readdirplus(req, ino, size, offset, fi);
//}

static void finesse_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi);
static void finesse_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi)
{
    finesse_set_provider(req, 0);
    return finesse_original_ops->create(req, parent, name, mode, fi);
}

static void finesse_fuse_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
static void finesse_fuse_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    finesse_set_provider(req, 0);
    return finesse_original_ops->open(req, ino, fi);
}

static void finesse_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
static void finesse_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    finesse_object_t *finobj = finesse_object_lookup_by_ino(ino);

    if (NULL != finobj)
    {
        /* basically, this is saying that this is no longer in use, so we remove the lookup reference */
        finesse_object_release(finobj);
    }

    finesse_set_provider(req, 0);
    req->finesse.notify = 1;
    return finesse_original_ops->release(req, ino, fi);
}

static void finesse_releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
static void finesse_releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    finesse_set_provider(req, 0);
    req->finesse.notify = 1;
    return finesse_original_ops->releasedir(req, ino, fi);
}

static void finesse_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi);
static void finesse_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi)
{
    finesse_set_provider(req, 0);
    return finesse_original_ops->read(req, ino, size, offset, fi);
}

// UNCOMMENT WRITE_BUF BEFORE RUNNING WORKLOADS ON OPTIMIZED STACKFS
//static void finesse_write_buf(fuse_req_t req, fuse_ino_t ino, struct fuse_bufvec *in_buf, off_t off, struct fuse_file_info *fi);
//static void finesse_write_buf(fuse_req_t req, fuse_ino_t ino, struct fuse_bufvec *in_buf, off_t off, struct fuse_file_info *fi)
//{
//    finesse_set_provider(req, 0);
//    return finesse_original_ops->write_buf(req, ino, in_buf, off, fi);
//}

static void finesse_fuse_unlink(fuse_req_t req, fuse_ino_t parent, const char *name)
{
  // This is my testing hack - to implement unlink here
    (void)parent;
    (void)name;
    fuse_reply_err(req, 0);
}

static void finesse_setattr(fuse_req_t req, fuse_ino_t nodeid, struct stat *attr, int to_set, struct fuse_file_info *fi);
static void finesse_setattr(fuse_req_t req, fuse_ino_t nodeid, struct stat *attr, int to_set, struct fuse_file_info *fi) 
{
    finesse_set_provider(req, 0);
    return finesse_original_ops->setattr(req, nodeid, attr, to_set, fi);
}

static void finesse_rmdir(fuse_req_t req, fuse_ino_t parent, const char* name);
static void finesse_rmdir(fuse_req_t req, fuse_ino_t parent, const char* name) 
{
    finesse_set_provider(req, 0);
    return finesse_original_ops->rmdir(req, parent, name);
}

static void finesse_write(fuse_req_t req, fuse_ino_t nodeid, const char *buf,
                          size_t size, off_t off, struct fuse_file_info *fi);
static void finesse_write(fuse_req_t req, fuse_ino_t nodeid, const char * buf,
                          size_t size, off_t off, struct fuse_file_info *fi) 
{
    finesse_set_provider(req, 0);
    req->finesse.notify = 1;
    return finesse_original_ops->write(req, nodeid, buf, size, off, fi);
}

//static void finesse_fuse_statfs(fuse_req_t req, const char *path);
//static void finesse_fuse_statfs(fuse_req_t req, const char *path) 
//{
//    finesse_set_provider(req, 0);
//    req->finesse.notify = 1;
//   return finesse_original_ops->statfs(req, path);
//}

static void finesse_fuse_fstatfs(fuse_req_t req, fuse_ino_t nodeid);
static void finesse_fuse_fstatfs(fuse_req_t req, fuse_ino_t nodeid) 
{
    finesse_set_provider(req, 0);
    req->finesse.notify = 1;
    return finesse_original_ops->statfs(req, nodeid);
}

static void finesse_fsync(fuse_req_t req, fuse_ino_t nodeid, int datasync, struct fuse_file_info *fi);
static void finesse_fsync(fuse_req_t req, fuse_ino_t nodeid, int datasync, struct fuse_file_info *fi) 
{
    finesse_set_provider(req, 0);
    return finesse_original_ops->fsync(req, nodeid, datasync, fi);
}


// Remove for now. Causes segfault when writing.
//static void finesse_getxattr(fuse_req_t req, fuse_ino_t nodeid, const char *name, size_t size);
//static void finesse_getxattr(fuse_req_t req, fuse_ino_t nodeid, const char *name, size_t size) 
//{
//    finesse_set_provider(req, 0);
//    return finesse_original_ops->getxattr(req, nodeid, name, size);
//}

static void finesse_flush(fuse_req_t req, fuse_ino_t nodeid, struct fuse_file_info *fi);
static void finesse_flush(fuse_req_t req, fuse_ino_t nodeid, struct fuse_file_info *fi) 
{
    finesse_set_provider(req, 0);
    return finesse_original_ops->flush(req, nodeid, fi);
}

static void finesse_forget_multi(fuse_req_t req, size_t count, struct fuse_forget_data *forgets);
static void finesse_forget_multi(fuse_req_t req, size_t count, struct fuse_forget_data *forgets)
{
    finesse_set_provider(req, 0);
    return finesse_original_ops->forget_multi(req, count, forgets);
}

static struct fuse_lowlevel_ops finesse_ops = {
    .init            = finesse_fuse_init,
    .lookup          = finesse_lookup,
    .getattr         = finesse_getattr,
    .forget          = finesse_forget,
    .readlink        = finesse_readlink,
    .unlink          = finesse_fuse_unlink,
    .opendir         = finesse_opendir,
    //.readdirplus     = finesse_readdirplus,
    .readdir         = finesse_readdir,
    .releasedir      = finesse_releasedir,
    .mkdir           = finesse_mkdir,
    .create          = finesse_create,
    .open            = finesse_fuse_open,
    .release         = finesse_release,
    .read            = finesse_read,
    //.write_buf       = finesse_write_buf,

    .setattr         = finesse_setattr,
    .rmdir           = finesse_rmdir,
    .write           = finesse_write,
    .statfs          = finesse_fuse_fstatfs,
    .fsync           = finesse_fsync,
    //.getxattr        = finesse_getxattr,
    .flush           = finesse_flush,
    .forget_multi    = finesse_forget_multi
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

    if (0 > FinesseStartServerConnection(&se->server_handle))
    {
        fprintf(stderr, "fuse (finesse): failed to start Finesse Server connection\n");
        se->server_handle = NULL;
    }

    return se;
}

/////
//
// given a file and a list of paths, find the first occurrence of the file in the path
//
// TODO: this is currently hacked to work with passthrough
//
#if 0
static char *find_file_in_path(struct fuse_session *session, char *file, char **paths, unsigned path_count)
{
    char *path_found = NULL;
    
    (void)session;
    (void)file;
    (void)paths;
    (void)path_count;
    unsigned path_index;
    struct fuse_req *req = finesse_alloc_req(session);
    size_t bufsize = 512;
    char *scratch_buffer;
    size_t mp_len = strlen(session->mountpoint);
    size_t fn_len = strlen(file);
    size_t out_buf_len;
    int used_prefix;
    finesse_lookup_info_t lookup_info;

    memset(&lookup_info, 0, sizeof(lookup_info));
    pthread_mutex_init(&lookup_info.lock, NULL);
    pthread_cond_init(&lookup_info.condition, NULL);

    if (NULL == req)
    {
        fprintf(stderr, "%s @ %d (%s): failed to connect to client %s\n", __FILE__, __LINE__, __FUNCTION__, strerror(errno));
        return NULL;
    }

    scratch_buffer = malloc(bufsize); 
    for (path_index = 0; path_index < path_count; path_index++)
    {
        size_t path_len = strlen(paths[path_index]);
        size_t name_len = mp_len + fn_len + path_len + (2 * sizeof(char));

        if (name_len < bufsize)
        {
            while (name_len < bufsize)
            {
                if (NULL != scratch_buffer)
                {
                    free(scratch_buffer);
                    scratch_buffer = NULL;
                }
                bufsize += 4096;
            }
            scratch_buffer = malloc(bufsize);
            if (NULL == scratch_buffer)
            {
                return NULL;
            }
        }

        if ((path_len < mp_len) || (0 != strncmp(paths[path_index], session->mountpoint, mp_len)))
        {
            snprintf(scratch_buffer, bufsize, "%s/%s/%s", session->mountpoint, paths[path_index], file);
            used_prefix = 1;
        }
        else
        {
            snprintf(scratch_buffer, bufsize, "%s/%s", paths[path_index], file);
            used_prefix = 0;
        }
        req->opcode = FUSE_LOOKUP + 128; // TODO: turn this into a define somewhere
        memset(&lookup_info.attr, 0, sizeof(lookup_info.attr));
        lookup_info.status = EINVAL;
        req->finesse_lookup_info = &lookup_info;
        finesse_original_ops->lookup(req, FUSE_ROOT_ID, scratch_buffer);
        pthread_mutex_lock(&lookup_info.lock);
        while (0 == lookup_info.attr.ino)
        {
            pthread_cond_wait(&lookup_info.condition, &lookup_info.lock);
        }
        pthread_mutex_unlock(&lookup_info.lock);

        // if it failed, try the next path
        if (0 != lookup_info.status)
        {
            continue;
        }

        /* is it a regular file? */
        if (!S_ISREG(lookup_info.attr.mode))
        {
            // TODO: is this reasonable semantics, or should we
            // allow non-files?  Maybe we should return a list of matches?  Bleh.
            continue; // nope - so we don't accept it
        }

        /* this means we found it */
        out_buf_len = strlen(scratch_buffer) + sizeof(char);
        if (used_prefix)
        {
            out_buf_len -= mp_len;
        }
        path_found = (char *)malloc(out_buf_len);

        if (NULL == path_found)
        {
            break; // can't do much here
        }

        if (used_prefix)
        {
            strcpy(path_found, &scratch_buffer[mp_len]);
        }
        else
        {
            strcpy(path_found, scratch_buffer);
        }

        // found it, so we are done.
        break;
    }

    if (NULL != scratch_buffer)
    {
        free(scratch_buffer);
        scratch_buffer = NULL;
    }

    if (NULL != req)
    {
        finesse_free_req(req);
        req = NULL;
    }

    return path_found;
}
#endif // 0

#if 0
static char *find_files_in_paths(struct fuse_session *session, char **file, unsigned file_count, char **paths, unsigned path_count)
{
    char *found = NULL;
    (void) session;
    (void) file;
    (void) file_count;
    (void) paths;
    (void) path_count;

    unsigned file_index;

    for (file_index = 0; (NULL == found) && (file_index < file_count); file_index++)
    {
        found = find_file_in_path(session, file[file_index], paths, path_count);
    }
    return found;
}
#endif // 0

static int handle_fuse_request(struct fuse_session *se, void *Client, fincomm_message Message)
{
    finesse_server_handle_t fsh = (finesse_server_handle_t)se->server_handle;
    finesse_msg *fmsg = NULL;

    if (NULL == fsh) {
        return ENOTCONN;
    }

    assert(FINESSE_REQUEST == Message->MessageType); // nothing else makes sense here
    assert(NULL != Message);
    fmsg = (finesse_msg *)Message->Data;
    assert(NULL != fmsg);
    assert(FINESSE_FUSE_MESSAGE == fmsg->MessageClass);

    // Now the big long switch statement
    switch (fmsg->Message.Fuse.Request.Type) {
        case FINESSE_FUSE_REQ_LOOKUP:
        case FINESSE_FUSE_REQ_FORGET:
        case FINESSE_FUSE_REQ_GETATTR:
        case FINESSE_FUSE_REQ_SETATTR:
        case FINESSE_FUSE_REQ_READLINK:
        case FINESSE_FUSE_REQ_MKNOD:
        case FINESSE_FUSE_REQ_MKDIR:
        case FINESSE_FUSE_REQ_UNLINK:
        case FINESSE_FUSE_REQ_RMDIR:
        case FINESSE_FUSE_REQ_SYMLINK:
        case FINESSE_FUSE_REQ_RENAME:
        case FINESSE_FUSE_REQ_LINK:
        case FINESSE_FUSE_REQ_OPEN:
        case FINESSE_FUSE_REQ_READ:
        case FINESSE_FUSE_REQ_WRITE:
        case FINESSE_FUSE_REQ_FLUSH:
        case FINESSE_FUSE_REQ_RELEASE:
        case FINESSE_FUSE_REQ_FSYNC:
        case FINESSE_FUSE_REQ_OPENDIR:
        case FINESSE_FUSE_REQ_READDIR:
        case FINESSE_FUSE_REQ_RELEASEDIR:
        case FINESSE_FUSE_REQ_FSYNCDIR:
        case FINESSE_FUSE_REQ_STATFS:
        case FINESSE_FUSE_REQ_SETXATTR:
        case FINESSE_FUSE_REQ_GETXATTR:
        case FINESSE_FUSE_REQ_LISTXATTR:
        case FINESSE_FUSE_REQ_REMOVEXATTR:
        case FINESSE_FUSE_REQ_ACCESS:
        case FINESSE_FUSE_REQ_CREATE:
        case FINESSE_FUSE_REQ_GETLK:
        case FINESSE_FUSE_REQ_SETLK:
        case FINESSE_FUSE_REQ_BMAP:
        case FINESSE_FUSE_REQ_IOCTL:
        case FINESSE_FUSE_REQ_POLL:
        case FINESSE_FUSE_REQ_WRITE_BUF:
        case FINESSE_FUSE_REQ_RETRIEVE_REPLY:
        case FINESSE_FUSE_REQ_FORGET_MULTI:
        case FINESSE_FUSE_REQ_FLOCK:
        case FINESSE_FUSE_REQ_FALLOCATE:
        case FINESSE_FUSE_REQ_READDIRPLUS:
        case FINESSE_FUSE_REQ_COPY_FILE_RANGE:
        case FINESSE_FUSE_REQ_LSEEK:
        default:
            fmsg->Message.Fuse.Response.Type = FINESSE_FUSE_RSP_ERR;
            fmsg->Message.Fuse.Response.Parameters.ReplyErr.Err = ENOTSUP;
            FinesseSendResponse(fsh, Client, Message);
            break;
    }

    return 0;
}

static int handle_native_name_map_request(struct fuse_session *se, void *Client, fincomm_message Message)
{
    finesse_server_handle_t fsh = (finesse_server_handle_t)se->server_handle;
    static uuid_t null_uuid;
    struct fuse_req *fuse_request;
    finesse_msg *fmsg = (finesse_msg *)Message->Data;
    int status = 0;
    size_t mp_length = strlen(se->mountpoint);

    if (NULL == fsh) {
        return ENOTCONN;
    }

    // Presently, we don't handle openat
    if (!uuid_is_null(fmsg->Message.Native.Request.Parameters.Map.Parent)) {
        return FinesseSendNameMapResponse(fsh, Client, Message, &null_uuid, ENOTSUP);
    }


    if (0 != strcmp(fmsg->Message.Native.Request.Parameters.Map.Name, se->mountpoint)) {
        return FinesseSendNameMapResponse(fsh, Client, Message, &null_uuid, ENOTDIR);      
    }

    // We need to do a lookup here
    fuse_request = finesse_alloc_req(se);

    if (NULL == fuse_request) {
        fprintf(stderr, "%s @ %d (%s): alloc failure\n", __FILE__, __LINE__, __FUNCTION__);
        // TODO: fix this function's prototype
        return FinesseSendNameMapResponse(fsh, Client, Message, &null_uuid, ENOMEM);      
    }

    fuse_request->finesse.message = Message;
    fuse_request->finesse.client = Client;
    finesse_original_ops->lookup(fuse_request, FUSE_ROOT_ID, &fmsg->Message.Native.Request.Parameters.Map.Name[mp_length]);

    return status;
 
}

static int handle_native_name_map_release_request(struct fuse_session *se, void *Client, fincomm_message Message)
{
    finesse_server_handle_t fsh = (finesse_server_handle_t)se->server_handle;
    finesse_object_t *object = NULL;
    finesse_msg *fmsg = (finesse_msg *)Message->Data;
    int status = 0;

    object = finesse_object_lookup_by_uuid(&fmsg->Message.Native.Request.Parameters.MapRelease.Key);
    if (NULL != object) {
        finesse_object_release(object);
    }

    if (NULL != fsh) {
        // TODO: fix this function prototype/call
        status = FinesseSendNameMapReleaseResponse(fsh, Client, Message, 0);
    }

    return status;
}


static int handle_native_request(struct fuse_session *se, void *Client, fincomm_message Message)
{
    finesse_msg *fmsg = NULL;
    int status = EINVAL;
    finesse_server_handle_t fsh = (finesse_server_handle_t)se->server_handle;

    if (NULL == fsh) {
        return ENOTCONN;
    }

    assert(FINESSE_REQUEST == Message->MessageType); // nothing else makes sense here
    assert(NULL != Message);
    fmsg = (finesse_msg *)Message->Data;
    assert(NULL != fmsg);
    assert(FINESSE_NATIVE_MESSAGE == fmsg->MessageClass);

    // Now the big long switch statement
    switch (fmsg->Message.Native.Request.NativeRequestType) {
        case FINESSE_NATIVE_REQ_TEST: {
            status = FinesseSendTestResponse(fsh, Client, Message, 0);
            if (0 > status)
            {
                perror("FinesseSendTestResponse");
            }
            break;

        }
        break;
        
        case FINESSE_NATIVE_REQ_MAP: {
            status = handle_native_name_map_request(se, Client, Message);
        }
        break;
        
        case FINESSE_NATIVE_REQ_MAP_RELEASE: {
            status = handle_native_name_map_release_request(se, Client, Message);
        }
        break;

        default:
            fmsg->Message.Native.Response.NativeResponseType = FINESSE_FUSE_RSP_ERR;
            fmsg->Message.Native.Response.Parameters.Err.Result = ENOTSUP;
            FinesseSendResponse(fsh, Client, Message);
            break;
    }

    return 0;
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
  
        if (NULL == fsh) {
            status = FinesseStartServerConnection(&fsh);
            assert(0 == status);
            assert(NULL != fsh);
        }

        status = FinesseGetRequest(fsh, &client, &request);
        assert(0 == status);
        assert(NULL != request);
        assert((uintptr_t)client < SHM_MESSAGE_COUNT);
        assert(0 != request->RequestId); // invalid request number

        assert(FINESSE_REQUEST == request->MessageType); // nothing else makes sense here
        fmsg = (finesse_msg *) request->Data;
        assert(NULL != fmsg);

        status = EINVAL;
        switch (fmsg->MessageClass) {
            default: {
                assert(0); // this shouldn't be happening.
            }
            break;

            case FINESSE_FUSE_MESSAGE: {
                status = handle_fuse_request(se, client, request);
            }
            break;
            
            case FINESSE_NATIVE_MESSAGE: {
                status = handle_native_request(se, client, request);
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

#if 0
static void *finesse_mq_worker(void *arg)
{
    struct fuse_session *se = (struct fuse_session *)arg;
    void *request;
    Finesse__FinesseRequest *finesse_req;
    void *client;
    int status;
    size_t mp_length = strlen(se->mountpoint);

    while ((0 < mp_length) && (NULL != se->server_handle))
    {

        status = FinesseGetRequest(se->server_handle, &client, &request);

        if (0 > status)
        {
            perror("FinesseGetRequest");
            break;
        }

        finesse_req = finesse__finesse_request__unpack(NULL, 0, request);

        switch (finesse_req->header->op)
        {
        default: // Unknown case
            break;

        case FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__TEST:
        {
            status = FinesseSendTestResponse(se->server_handle, (uuid_t *)finesse_req->clientuuid.data, finesse_req->header->messageid, 0);
            if (0 > status)
            {
                perror("FinesseSendTestResponse");
            }
            break;
        }
        case FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__NAME_MAP:
        {
            static uuid_t dummy_uuid;
            struct fuse_req *fuse_request;

            if (0 != strcmp(finesse_req->namemapreq->name, se->mountpoint))
            {

                status = FinesseSendNameMapResponse(se->server_handle, (uuid_t *)finesse_req->clientuuid.data, finesse_req->header->messageid, &dummy_uuid, ENOTDIR);
                break;
            }

            /* do a lookup */
            fprintf(stderr, "finesse (fuse): map name request for %s\n", finesse_req->namemapreq->name);
            fprintf(stderr, "finesse (fuse): mount point is %s (len = %zu)\n", se->mountpoint, mp_length);
            fprintf(stderr, "finesse (fuse): do lookup on %s\n", &finesse_req->namemapreq->name[mp_length]);
            fuse_request = finesse_alloc_req(se);
            if (NULL == fuse_request)
            {
                fprintf(stderr, "%s @ %d (%s): alloc failure\n", __FILE__, __LINE__, __FUNCTION__);
                status = FinesseSendNameMapResponse(se->server_handle, (uuid_t *)finesse_req->clientuuid.data, finesse_req->header->messageid, &dummy_uuid, ENOMEM);
                break;
            }
            fuse_request->finesse_req = finesse_req;
            finesse_original_ops->lookup(fuse_request, FUSE_ROOT_ID, &finesse_req->namemapreq->name[mp_length + 1]);
            finesse_req = NULL;
            break;
        }
        case FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__NAME_MAP_RELEASE:
        {
            finesse_object_t *object = NULL;

            object = finesse_object_lookup_by_uuid((uuid_t *)finesse_req->namemapreleasereq->key.data);
            if (NULL != object)
            {
                finesse_object_release(object);
            }

            status = FinesseSendNameMapReleaseResponse(se->server_handle, (uuid_t *)finesse_req->clientuuid.data, finesse_req->header->messageid, 0);
            break;
        }
        case FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__DIR_MAP:
            break;
        case FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__DIR_MAP_RELEASE:
            break;
        case FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__UNLINK:
        {
            /* HACK */
            /* since we're testing passthrough_ll and it doesn't support unlink, we do it here directly */
            status = FinesseSendUnlinkResponse(se->server_handle, (uuid_t *)finesse_req->clientuuid.data, finesse_req->header->messageid, unlink(finesse_req->unlinkreq->name));
            // end gross hack
            break;
        }
        //case FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__STATFS:
        //{
	//    struct statvfs fs;
	//    int64_t result = finesse_original_ops->statfs(finesse_req->statfsreq->path, &fs);
        //    status = FinesseSendStatfsResponse(se->server_handle, (uuid_t *)finesse_req->clientuuid.data, finesse_req->header->messageid, &fs, result);
        //    break;
        //}
        case FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__FSTATFS:
        {
            struct statvfs fs;
            int64_t result = fstatvfs(finesse_req->fstatfsreq->nodeid, &fs);
            status = FinesseSendFstatfsResponse(se->server_handle, (uuid_t *)finesse_req->clientuuid.data, finesse_req->header->messageid, &fs, result);
            break;
        }
        case FINESSE__FINESSE_MESSAGE_HEADER__OPERATION__PATH_SEARCH:
        {
            char *path_found = find_files_in_paths(se,
                                                   finesse_req->pathsearchreq->files,
                                                   finesse_req->pathsearchreq->n_files,
                                                   finesse_req->pathsearchreq->paths,
                                                   finesse_req->pathsearchreq->n_paths);

            if (NULL == path_found)
            {
                status = ENOENT;
            }
            else
            {
                status = 0;
            }

            status = FinesseSendPathSearchResponse(se->server_handle, (uuid_t *)finesse_req->clientuuid.data, finesse_req->header->messageid, path_found, status);

            if (NULL != path_found)
            {
                free(path_found);
                path_found = NULL;
            }

            break;
        }

        } // end case

        // cleanup:
        if (NULL != finesse_req)
        {
            finesse__finesse_request__free_unpacked(finesse_req, NULL);
            finesse_req = NULL;
        }

        if (NULL != request)
        {
            FinesseFreeRequest(se->server_handle, request);
            request = NULL;
        }
    }

    return NULL;
    (void)finesse_alloc_req(NULL);
}
#endif // 0


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
        // no action is the default
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
        break;
    }
    case FUSE_RELEASE:
    {
        // this is done in the pre-call release path
    }
    case FUSE_OPENDIR:
    {
        // TODO
        break;
    }
    case FUSE_RELEASEDIR:
    {
        // TODO
        break;
    }
    case FUSE_STATFS:
    	break;
    }
    return;
}

int finesse_send_reply_iov(fuse_req_t req, int error, struct iovec *iov, int count, int free_req)
{
    struct fuse_out_header out;
    struct fuse_entry_out *arg = NULL;

    if (error <= -1000 || error > 0)
    {
        fprintf(stderr, "fuse: bad error value: %i\n", error);
        error = -ERANGE;
    }

    out.unique = req->unique;
    out.error = error;

    iov[0].iov_base = &out;
    iov[0].iov_len = sizeof(struct fuse_out_header);

    if (count > 1)
    {
        arg = (struct fuse_entry_out *)iov[1].iov_base;
    }

    if ((req->opcode > 127) && (req->opcode < 1024))
    {
        /* this is one of our "internal" operations */
        switch (req->opcode)
        {
        default:
            // no idea what is being requested here
            assert(0);
            break;
        case FUSE_LOOKUP + 128: // TODO - make this a manifest constant
        {
            finesse_lookup_info_t *lookup_info = (finesse_lookup_info_t *)req->finesse.lookup_info;

            // this is the "lookup for path search" code
            free_req = 0; // do not free this request - it gets reused
            lookup_info->status = error;
            if (NULL != arg)
            {
                lookup_info->attr = arg->attr;
            }
            else
            {
                assert(0 != error); // otherwise I have no idea what this means
            }
            // Expectation is that this will always be uncontended
            pthread_mutex_lock(&lookup_info->lock);
            pthread_cond_signal(&lookup_info->condition);
            pthread_mutex_unlock(&lookup_info->lock);
            /* request ownership is returned to the original thread */
            req = NULL;
            break;
        }

        } // end switch
    }
    else
    {
        // don't know what is being asked here, so abort
        assert(0);
    }

    // cleanup
    if (NULL != req)
    {
        if (NULL != req->finesse.message)
        {
            assert(0); // really should have set this to NULL when we sent the response
            req->finesse.message = NULL;
        }

        if (free_req)
        {
            finesse_free_req(req);
        }
    }

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
