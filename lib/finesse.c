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
#include "finesse_msg.h"
#include "finesse-fuse.h"
#include "finesse-lookup.h"
#include "finesse-list.h"
#include "finesse.h"

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

typedef struct finesse_client_mq_map
{
    struct finesse_client_mq_map *next;
    struct finesse_client_mq_map *prev;
    uuid_t uuid;
    mqd_t mq_descriptor;
} finesse_client_mq_map_t;

/* protect the client lookup info */
pthread_rwlock_t finesse_client_mq_map_lock = PTHREAD_RWLOCK_INITIALIZER;
finesse_client_mq_map_t *finesse_client_mq_map_list;
unsigned int finesse_client_mq_map_count = 0;

static mqd_t finesse_lookup_mq_for_client_locked(uuid_t clientUuid)
{
    finesse_client_mq_map_t *map;
    mqd_t mq_descriptor = -ENOENT;

    if (NULL == finesse_client_mq_map_list)
    {
        return -ENOENT;
    }

    map = finesse_client_mq_map_list;
    do
    {
        if (0 == uuid_compare(clientUuid, map->uuid))
        {
            /* we found it */
            mq_descriptor = map->mq_descriptor;
            break;
        }
        map = map->next;
    } while (map != finesse_client_mq_map_list);

    return mq_descriptor;
}

static mqd_t finesse_lookup_mq_for_client(uuid_t clientUuid)
{
    mqd_t mq_descriptor = -ENOENT;

    if (NULL == finesse_client_mq_map_list)
    {
        return -ENOENT;
    }

    pthread_rwlock_rdlock(&finesse_client_mq_map_lock);
    mq_descriptor = finesse_lookup_mq_for_client_locked(clientUuid);
    pthread_rwlock_unlock(&finesse_client_mq_map_lock);

    return mq_descriptor;
}

static void remove_client_mq_map(finesse_client_mq_map_t *map)
{
    map->next->prev = map->prev;
    map->prev->next = map->next;
    map->next = map->prev = map;
    if (0 > mq_close(map->mq_descriptor))
    {
        fprintf(stderr, "%d @ %s (%s) FAILED TO CLOSE descriptor %d (%s)\n",
                __LINE__, __FILE__, __FUNCTION__, (int)map->mq_descriptor, strerror(errno));
    }
    else
    {
        fprintf(stderr, "%d @ %s (%s) closed descriptor %d\n",
                __LINE__, __FILE__, __FUNCTION__, (int)map->mq_descriptor);
    }
    free(map);
    finesse_client_mq_map_count--;
}

#if 0
static int finesse_remove_mq_for_client_locked(uuid_t clientUuid)
{
	finesse_client_mq_map_t *map = NULL;
	int status = ENOENT;

	pthread_rwlock_wrlock(&finesse_client_mq_map_lock);

	for (map = finesse_client_mq_map_list->next;
	     map != finesse_client_mq_map_list;
		 map = map->next) {

		if (0 == memcmp(map->uuid, &clientUuid, sizeof(uuid_t))) {
			remove_client_mq_map(map);
			status = 0;
			break;
		}
	}

	pthread_rwlock_unlock(&finesse_client_mq_map_lock);
	
	return status;
}
#endif // 0

static int finesse_insert_mq_for_client(uuid_t clientUuid, mqd_t mq_descriptor)
{
    finesse_client_mq_map_t *map = malloc(sizeof(finesse_client_mq_map_t));
    int status = 0;

    if (NULL == map)
    {
        return -ENOMEM;
    }

    map->next = map;
    map->prev = map;
    uuid_copy(map->uuid, clientUuid);
    map->mq_descriptor = mq_descriptor;

    pthread_rwlock_wrlock(&finesse_client_mq_map_lock);

    if (NULL == finesse_client_mq_map_list)
    {
        finesse_client_mq_map_list = map;
    }

    //
    // TODO: parameterize this and adjust it - there's a low limit to the
    // number of message queues that are allowed to be opened.
    //
    if (finesse_client_mq_map_count > 5)
    {
        remove_client_mq_map(finesse_client_mq_map_list->prev); // prune the oldest one
    }

    while (finesse_client_mq_map_list != map)
    {
        if (finesse_lookup_mq_for_client_locked(clientUuid) >= 0)
        {
            /* already exists! */
            status = -EEXIST;
            break;
        }

        map->next = finesse_client_mq_map_list;
        map->prev = finesse_client_mq_map_list->prev;
        finesse_client_mq_map_list->prev->next = map;
        finesse_client_mq_map_list->prev = map;
        finesse_client_mq_map_list = map;
        finesse_client_mq_map_count++;
        break;
    }
    pthread_rwlock_unlock(&finesse_client_mq_map_lock);

    return status;
}

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

#if 0
// TODO: figure out why I can't use the version in libfuse itself.  Frustrating!
void fuse_chan_put(struct fuse_chan *ch)
{
    if (ch == NULL)
        return;
    pthread_mutex_lock(&ch->lock);
    ch->ctr--;
    if (!ch->ctr)
    {
        pthread_mutex_unlock(&ch->lock);
        close(ch->fd);
        pthread_mutex_destroy(&ch->lock);
        free(ch);
    }
    else
        pthread_mutex_unlock(&ch->lock);
}
#endif // 0

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
    req->finesse_notify = 1;
    /* TODO: add to lookup table? */
    return finesse_original_ops->lookup(req, parent, name);
}

static void finesse_forget(fuse_req_t req, fuse_ino_t ino, uint64_t nlookup);
static void finesse_forget(fuse_req_t req, fuse_ino_t ino, uint64_t nlookup)
{
    finesse_set_provider(req, 0);
    req->finesse_notify = 1;
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
    req->finesse_notify = 1;
    return finesse_original_ops->opendir(req, ino, fi);
}

static void finesse_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi);
static void finesse_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi)
{
    finesse_set_provider(req, 0);
    return finesse_original_ops->readdir(req, ino, size, offset, fi);
}

static void finesse_readdirplus(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi);
static void finesse_readdirplus(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi)
{
    finesse_set_provider(req, 0);
    return finesse_original_ops->readdirplus(req, ino, size, offset, fi);
}

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
    finesse_set_provider(req, 0);
    req->finesse_notify = 1;
    return finesse_original_ops->release(req, ino, fi);
}

static void finesse_releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
static void finesse_releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    finesse_set_provider(req, 0);
    req->finesse_notify = 1;
    return finesse_original_ops->releasedir(req, ino, fi);
}

static void finesse_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi);
static void finesse_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi)
{
    finesse_set_provider(req, 0);
    return finesse_original_ops->read(req, ino, size, offset, fi);
}

static void finesse_write_buf(fuse_req_t req, fuse_ino_t ino, struct fuse_bufvec *in_buf, off_t off, struct fuse_file_info *fi);
static void finesse_write_buf(fuse_req_t req, fuse_ino_t ino, struct fuse_bufvec *in_buf, off_t off, struct fuse_file_info *fi)
{
    finesse_set_provider(req, 0);
    return finesse_original_ops->write_buf(req, ino, in_buf, off, fi);
}

static void finesse_unlink(fuse_req_t req, fuse_ino_t parent, const char *name)
{
    // This is my testing hack - to implement unlink here
    (void)parent;
    (void)name;
    fuse_reply_err(req, 0);
}

static struct fuse_lowlevel_ops finesse_ops = {
    .init = finesse_fuse_init,
    .lookup = finesse_lookup,
    .forget = finesse_forget,
    .getattr = finesse_getattr,
    .readlink = finesse_readlink,
    .unlink = finesse_unlink,
    .opendir = finesse_opendir,
    .readdir = finesse_readdir,
    .readdirplus = finesse_readdirplus,
    .releasedir = finesse_releasedir,
    .create = finesse_create,
    .open = finesse_fuse_open,
    .release = finesse_release,
    .read = finesse_read,
    .write_buf = finesse_write_buf};

uuid_t finesse_server_uuid;

/**
 * Set the provider for the given request.
 */
void finesse_set_provider(fuse_req_t req, int finesse)
{
    if (finesse)
    {
        req->finesse = 1;
    }
    else
    {
        req->finesse = 0;
    }
}

int finesse_get_provider(fuse_req_t req)
{
    return req->finesse;
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

static int finesse_connect_to_client(uuid_t clientUuid)
{
    char name[64];

    /* TODO parameterize this name */
    strncpy(name, "/finesse_", sizeof(name));
    uuid_unparse_lower(clientUuid, &name[strlen(name)]);
    return mq_open(name, O_WRONLY);
}

static int finesse_send_response(uuid_t clientUuid, finesse_message_t *response)
{
    mqd_t mq_descriptor = finesse_lookup_mq_for_client(clientUuid);
    size_t bytes_to_send;

    if (mq_descriptor == -ENOENT)
    {
        /* create it */
        mq_descriptor = finesse_connect_to_client(clientUuid);

        if (mq_descriptor >= 0)
        {
            fprintf(stderr, "%s @ %d (%s): open mq %d\n", __FILE__, __LINE__, __FUNCTION__, (int)mq_descriptor);
            finesse_insert_mq_for_client(clientUuid, mq_descriptor);
        }
    }

    if (mq_descriptor < 0)
    {
        fprintf(stderr, "%s @ %d (%s): failed to connect to client %s\n", __FILE__, __LINE__, __FUNCTION__, strerror(errno));
        return mq_descriptor;
    }

    // don't send less than the smallest sized message
    // TODO: should we adjust the receiver to allow no message data? Probably safer.
    bytes_to_send = offsetof(finesse_message_t, Message) + response->MessageLength;

    if (bytes_to_send < sizeof(finesse_message_t))
    {
        bytes_to_send = sizeof(finesse_message_t);
    }

    return mq_send(mq_descriptor, (char *)response, bytes_to_send, 0);
}

static void *finesse_mq_worker(void *arg)
{
#if 1
    struct fuse_session *se = (struct fuse_session *)arg;

    (void)se;
#else
    struct fuse_session *se = (struct fuse_session *)arg;
    struct mq_attr attr;
    ssize_t bytes_received, bytes_to_send;
    finesse_message_t *finesse_request, *finesse_response;
    struct fuse_req *req;

    if (NULL == se)
    {
        pthread_exit(NULL);
    }

    if (mq_getattr(se->message_queue_descriptor, &attr) < 0)
    {
        fprintf(stderr, "finesse (fuse): failed to get message queue attributes: %s\n", strerror(errno));
        return NULL;
    }

    finesse_response = malloc(attr.mq_msgsize);
    if (NULL == finesse_response)
    {
        fprintf(stderr, "finesse (fuse): failed to allocate response buffer: %s\n", strerror(errno));
        return NULL;
    }

    finesse_request = malloc(attr.mq_msgsize);

    while (NULL != finesse_request)
    {

        bytes_to_send = 0;

        bytes_received = mq_receive(se->message_queue_descriptor, (char *)finesse_request, attr.mq_msgsize, NULL /* optional priority */);

        if (bytes_received < 0)
        {
            fprintf(stderr, "finesse (fuse): failed to read message from queue: %s\n", strerror(errno));
            return NULL;
        }

        if (bytes_received < offsetof(finesse_message_t, Message))
        {
            // this message isn't even big enough for us to process
            fprintf(stderr, "finesse (fuse): short message received from queue\n");
            break;
        }

        /* now we have a message from the queue and need to process it */
        if (0 != memcmp(FINESSE_MESSAGE_MAGIC, finesse_request->MagicNumber, FINESSE_MESSAGE_MAGIC_SIZE))
        {
            /* not a valid message */
            fprintf(stderr, "finesse (fuse): invalid message received from queue\n");
            break;
        }

        fprintf(stderr, "finesse (fuse): received message\n");

        /* handle the request */
        switch (finesse_request->MessageType)
        {
        default:
        {
            fprintf(stderr, "finesse (fuse): invalid message type received %d\n", (int)finesse_request->MessageType);
            break;
        }

        case FINESSE_FUSE_OP_RESPONSE:
        case FINESSE_NAME_MAP_RESPONSE:
        case FINESSE_FUSE_NOTIFY:
        {
            fprintf(stderr, "finesse (fuse): not implemented type received %d\n", (int)finesse_request->MessageType);
            break;
        }

        case FINESSE_NAME_MAP_REQUEST:
        {
            /* first, is the request here a match for this file system? */
            size_t message_length = finesse_request->MessageLength + offsetof(finesse_message_t, Message);
            size_t mp_length = strlen(se->mountpoint);

            if ((bytes_received < message_length) ||
                (0 != strncmp(finesse_request->Message, se->mountpoint, mp_length)))
            {
                /* this is not ours */
                memcpy(finesse_response->MagicNumber, FINESSE_MESSAGE_MAGIC, FINESSE_MESSAGE_MAGIC_SIZE);
                memcpy(&finesse_response->SenderUuid, finesse_server_uuid, sizeof(uuid_t));
                finesse_response->MessageType = FINESSE_NAME_MAP_RESPONSE;
                finesse_response->MessageId = finesse_request->MessageId;
                finesse_response->MessageLength = sizeof(finesse_name_map_response_t);
                ((finesse_name_map_response_t *)finesse_response->Message)->Status = FINESSE_MAP_RESPONSE_INVALID;
                // bytes_to_send = offsetof(finesse_message_t, Message) + sizeof(finesse_name_map_response_t);
                fprintf(stderr, "%s @ %d (%s): invalid request\n", __FILE__, __LINE__, __FUNCTION__);
                break;
            }

            /* so let's do a lookup */
            /* map the name given */
            fprintf(stderr, "finesse (fuse): map name request for %s\n", finesse_request->Message);
            fprintf(stderr, "finesse (fuse): mount point is %s (len = %zu)\n", se->mountpoint, mp_length);
            fprintf(stderr, "finesse (fuse): do lookup on %s\n", &finesse_request->Message[mp_length]);
            req = finesse_alloc_req(se);
            if (NULL == req)
            {
                fprintf(stderr, "%s @ %d (%s): alloc failure\n", __FILE__, __LINE__, __FUNCTION__);
                break;
            }
            req->finesse_req = finesse_request;
            req->finesse_rsp = finesse_response;
            finesse_response = NULL; /* again, passing it to the lookup, consume it in the completion handler */
            finesse_original_ops->lookup(req, FUSE_ROOT_ID, &finesse_request->Message[mp_length + 1]);
            finesse_request = NULL; /* passing it to the lookup */
            break;
        }

        case FINESSE_MAP_RELEASE_REQUEST:
        {
            size_t message_length = sizeof(finesse_map_release_request_t) + offsetof(finesse_message_t, Message);
            finesse_map_release_request_t *mrreq = (finesse_map_release_request_t *)finesse_request->Message;
            finesse_object_t *object = NULL;

            // format generic response info
            memcpy(finesse_response->MagicNumber, FINESSE_MESSAGE_MAGIC, FINESSE_MESSAGE_MAGIC_SIZE);
            memcpy(&finesse_response->SenderUuid, finesse_server_uuid, sizeof(uuid_t));
            finesse_response->MessageType = FINESSE_MAP_RELEASE_RESPONSE;
            finesse_response->MessageId = finesse_request->MessageId;
            finesse_response->MessageLength = sizeof(finesse_map_release_response_t);
            bytes_to_send = offsetof(finesse_message_t, Message) + sizeof(finesse_map_release_response_t);

            if ((message_length > bytes_received) ||                       // short message
                (mrreq->Key.KeyLength > finesse_request->MessageLength) || // short message
                (mrreq->Key.KeyLength != sizeof(uuid_t)))
            { // invalid key length
                ((finesse_map_release_response_t *)finesse_response->Message)->Status = FINESSE_MAP_RESPONSE_INVALID;
                fprintf(stderr, "%s @ %d (%s): invalid request\n", __FILE__, __LINE__, __FUNCTION__);
                break;
            }

            // lookup
            object = finesse_object_lookup_by_uuid((uuid_t *)mrreq->Key.Key);
            if (NULL != object)
            {
                finesse_object_release(object);
            }
            ((finesse_map_release_response_t *)finesse_response->Message)->Status = FINESSE_MAP_RESPONSE_SUCCESS;
            break;
        }

        case FINESSE_DIR_MAP_REQUEST:
        {
            break;
        }

        case FINESSE_FUSE_OP_REQUEST:
        {
            break;
        }

        case FINESSE_UNLINK_REQUEST:
        {
            size_t message_length = sizeof(finesse_map_release_request_t) + offsetof(finesse_message_t, Message);
            finesse_unlink_request_t *ulreq = (finesse_unlink_request_t *)finesse_request->Message;
            finesse_unlink_response_t *ulrsp = (finesse_unlink_response_t *)finesse_response->Message;

            // format generic response info
            memcpy(finesse_response->MagicNumber, FINESSE_MESSAGE_MAGIC, FINESSE_MESSAGE_MAGIC_SIZE);
            memcpy(&finesse_response->SenderUuid, finesse_server_uuid, sizeof(uuid_t));
            finesse_response->MessageType = FINESSE_UNLINK_RESPONSE;
            finesse_response->MessageId = finesse_request->MessageId;
            finesse_response->MessageLength = sizeof(finesse_unlink_response_t);
            bytes_to_send = offsetof(finesse_message_t, Message) + sizeof(finesse_unlink_response_t);

            if (message_length > bytes_received)
            { // invalid key length
                ((finesse_unlink_response_t *)finesse_response->Message)->Status = FINESSE_MAP_RESPONSE_INVALID;
                fprintf(stderr, "%s @ %d (%s): invalid request\n", __FILE__, __LINE__, __FUNCTION__);
                break;
            }

            //
            // TODO: this is a sleazy hack - I'm using passthrough which is read-only, but I want to test
            //       unlink which isn't supported.
            ulrsp->Status = unlink(&ulreq->Name[strlen(se->mountpoint)]);
            // end gross hack

            break;
        }

        case FINESSE_PATH_SEARCH_REQUEST:
        {
            // finesse_path_search_request_t *search_message = (finesse_path_search_request_t *)finesse_request->Message;
            finesse_path_search_response_t *search_response = NULL;
            ssize_t response_length = offsetof(finesse_message_t, Message) + sizeof(finesse_path_search_response_t);

            memcpy(finesse_response->MagicNumber, FINESSE_MESSAGE_MAGIC, FINESSE_MESSAGE_MAGIC_SIZE);
            memcpy(&finesse_response->SenderUuid, finesse_server_uuid, sizeof(uuid_t));
            finesse_response->MessageType = FINESSE_PATH_SEARCH_RESPONSE;
            finesse_response->MessageId = finesse_request->MessageId;

            if (response_length < finesse_request->MessageLength)
            {
                // we received a runt request
                finesse_response->MessageLength = 0;
                bytes_to_send = sizeof(finesse_message_t);
                break;
            }

            // TODO: implement this functionality

            /* send response */
            finesse_response->MessageLength = sizeof(finesse_path_search_response_t);
            search_response = (finesse_path_search_response_t *)finesse_response->Message;
            search_response->Status = ENOSYS; // TODO: rationalize these error codes
            search_response->PathLength = 0;
            search_response->Path[0] = '\0';
            bytes_to_send = response_length;

            break;
        }

        case FINESSE_TEST:
        {
            memcpy(finesse_response->MagicNumber, FINESSE_MESSAGE_MAGIC, FINESSE_MESSAGE_MAGIC_SIZE);
            memcpy(&finesse_response->SenderUuid, finesse_server_uuid, sizeof(uuid_t));
            finesse_response->MessageType = FINESSE_TEST_RESPONSE;
            finesse_response->MessageId = finesse_request->MessageId;

            //// test code
            finesse_test_search();
            /// end test code

            /* send response */
            finesse_response->MessageLength = 0;
            finesse_response->Message[0] = '\0';
            bytes_to_send = sizeof(finesse_message_t);

            break;
        }
        }

        if (NULL == finesse_request)
        {
            /* need a new one - must have consumed it */
            finesse_request = (finesse_message_t *)malloc(attr.mq_msgsize);
        }

        if (NULL == finesse_response)
        {
            finesse_response = (finesse_message_t *)malloc(attr.mq_msgsize);
            assert(0 == bytes_to_send);
            bytes_to_send = 0;
        }

        if (0 < bytes_to_send)
        {
            uuid_t uuid;
            fprintf(stderr, "finesse (fuse): sending response (size = %zu)\n", bytes_to_send);
            memcpy(&uuid, &finesse_request->SenderUuid, sizeof(uuid_t));
            finesse_send_response(uuid, finesse_response);
        }
    }

    if (NULL != finesse_response)
    {
        free(finesse_response);
        finesse_response = NULL;
    }
#endif // 0

    return NULL;
    (void)finesse_alloc_req(NULL);
}

#if 0
	FUSE_LOOKUP	   = 1,
	FUSE_FORGET	   = 2,  /* no reply */
	FUSE_GETATTR	   = 3,
	FUSE_SETATTR	   = 4,
	FUSE_READLINK	   = 5,
	FUSE_SYMLINK	   = 6,
	FUSE_MKNOD	   = 8,
	FUSE_MKDIR	   = 9,
	FUSE_UNLINK	   = 10,
	FUSE_RMDIR	   = 11,
	FUSE_RENAME	   = 12,
	FUSE_LINK	   = 13,
	FUSE_OPEN	   = 14,
	FUSE_READ	   = 15,
	FUSE_WRITE	   = 16,
	FUSE_STATFS	   = 17,
	FUSE_RELEASE       = 18,
	FUSE_FSYNC         = 20,
	FUSE_SETXATTR      = 21,
	FUSE_GETXATTR      = 22,
	FUSE_LISTXATTR     = 23,
	FUSE_REMOVEXATTR   = 24,
	FUSE_FLUSH         = 25,
	FUSE_INIT          = 26,
	FUSE_OPENDIR       = 27,
	FUSE_READDIR       = 28,
	FUSE_RELEASEDIR    = 29,
	FUSE_FSYNCDIR      = 30,
	FUSE_GETLK         = 31,
	FUSE_SETLK         = 32,
	FUSE_SETLKW        = 33,
	FUSE_ACCESS        = 34,
	FUSE_CREATE        = 35,
	FUSE_INTERRUPT     = 36,
	FUSE_BMAP          = 37,
	FUSE_DESTROY       = 38,
	FUSE_IOCTL         = 39,
	FUSE_POLL          = 40,
	FUSE_NOTIFY_REPLY  = 41,
	FUSE_BATCH_FORGET  = 42,
	FUSE_FALLOCATE     = 43,
	FUSE_READDIRPLUS   = 44,
	FUSE_RENAME2       = 45,
	FUSE_LSEEK         = 46,

#endif // 0

void finesse_notify_reply_iov(fuse_req_t req, int error, struct iovec *iov, int count)
{
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

        assert(iov[1].iov_len >= sizeof(struct fuse_entry_out));
        ino = arg->nodeid;
        // TODO: figure out how to insert this
        nicobj = finesse_object_create(ino, NULL);
        assert(NULL != nicobj);
        finesse_object_release(nicobj);
        break;
    }
    case FUSE_RELEASE:
    {
        // TODO
        break;
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
    }
    return;
}

int finesse_send_reply_iov(fuse_req_t req, int error, struct iovec *iov, int count, int free_req)
{
    struct fuse_out_header out;
    finesse_message_t *finesse_request = req->finesse_req;
    finesse_message_t *finesse_response = req->finesse_rsp;
    size_t bytes_to_send = 0;

    (void)count;

    if (error <= -1000 || error > 0)
    {
        fprintf(stderr, "fuse: bad error value: %i\n", error);
        error = -ERANGE;
    }

    out.unique = req->unique;
    out.error = error;

    iov[0].iov_base = &out;
    iov[0].iov_len = sizeof(struct fuse_out_header);

    /* return fuse_send_msg(req->se, req->ch, iov, count); */

    switch (finesse_request->MessageType)
    {
    default:
        // don't know what is being asked here, so abort
        assert(0);
        break;

    case FINESSE_NAME_MAP_REQUEST:
    {
        finesse_name_map_response_t *nmr;

        assert(NULL != finesse_request);
        assert(NULL != finesse_response);

        /* let's set up the response here */
        memcpy(finesse_response->MagicNumber, FINESSE_MESSAGE_MAGIC, FINESSE_MESSAGE_MAGIC_SIZE);
        memcpy(&finesse_response->SenderUuid, finesse_server_uuid, sizeof(uuid_t));
        finesse_response->MessageType = FINESSE_NAME_MAP_RESPONSE;
        finesse_response->MessageId = finesse_request->MessageId;
        finesse_response->MessageLength = sizeof(finesse_name_map_response_t);
        ((finesse_name_map_response_t *)finesse_response->Message)->Status = FINESSE_MAP_RESPONSE_INVALID;

        nmr = (finesse_name_map_response_t *)finesse_response->Message;

        while (0 == error)
        {
            struct fuse_entry_out *arg = (struct fuse_entry_out *)iov[1].iov_base;
            finesse_object_t *nobj;

            nobj = finesse_object_create((ino_t)arg->nodeid, NULL);

            if (NULL == nobj)
            {
                error = ENOMEM;
                break;
            }

            nmr->Status = FINESSE_MAP_RESPONSE_SUCCESS;
            nmr->Key.KeyLength = sizeof(uuid_t);
            memcpy(nmr->Key.Key, &nobj->uuid, sizeof(uuid_t));
            finesse_response->MessageLength = offsetof(finesse_name_map_response_t, Key.Key) + nmr->Key.KeyLength;
            finesse_object_release(nobj);
            break;
        }

        if (0 != error)
        {
            nmr->Status = FINESSE_MAP_RESPONSE_INVALID;
            nmr->Key.KeyLength = 0;
        }

        bytes_to_send = offsetof(finesse_message_t, Message);
        bytes_to_send += finesse_response->MessageLength;
    }
    break;
    }

    if (0 < bytes_to_send)
    {
        uuid_t uuid;
        fprintf(stderr, "finesse (fuse): sending response %zu (%d @ %s)\n", bytes_to_send, __LINE__, __FILE__);
        memcpy(&uuid, &finesse_request->SenderUuid, sizeof(uuid_t));
        finesse_send_response(uuid, finesse_response);
    }

    if (NULL != req->finesse_req)
    {
        free(req->finesse_req);
        req->finesse_req = NULL;
    }

    if (NULL != req->finesse_rsp)
    {
        free(req->finesse_rsp);
        req->finesse_rsp = NULL;
    }

    if (free_req)
    {
        finesse_free_req(req);
    }

    return 0;
}

// static struct sigevent finesse_mq_sigevent;
static pthread_attr_t finesse_mq_thread_attr;
#define FINESSE_MAX_THREADS (1)
pthread_t finesse_threads[FINESSE_MAX_THREADS];
#undef fuse_session_loop_mt

int finesse_session_loop_mt_32(struct fuse_session *se, struct fuse_loop_config *config)
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
            status = pthread_create(&finesse_threads[index], &finesse_mq_thread_attr, finesse_mq_worker, se);
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
    return finesse_session_loop_mt_32(se, &config);
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
