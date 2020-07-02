//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include "bitbucketcalls.h"
#include <errno.h>
#include <pthread.h>
#include <malloc.h>

static int bitbucket_internal_forget(fuse_req_t req, fuse_ino_t ino, uint64_t nlookup);

static list_entry_t background_cleanup_queue = { 
	.next = &background_cleanup_queue, 
	.prev = &background_cleanup_queue
};
static pthread_mutex_t background_cleanup_queue_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t background_cleanup_queue_cond = PTHREAD_COND_INITIALIZER;
pthread_t background_thread;
int background_thread_started = 0;
int background_thread_shutdown = 0;

#define BACKGROUND_FORGET_COUNT_MAX (64)

typedef struct _background_forget_work {
	list_entry_t 		  ListEntry;
	uint8_t 	 		  Count;
	bitbucket_userdata_t *BBud;
	struct {
		ino_t 	InodeToForget;
		size_t	Bias; // how many times do we need to forget it?
	} ThingsToForget[BACKGROUND_FORGET_COUNT_MAX];
} background_forget_work_t;

static void background_forget(bitbucket_userdata_t *BBud, fuse_ino_t ino, uint64_t nlookup)
{
	bitbucket_inode_t *inode = NULL;

	inode = BitbucketLookupInodeInTable(BBud->InodeTable, ino);

	if (NULL != inode) {

		for (uint64_t index = 0; index < nlookup; index++) {
			BitbucketDereferenceInode(inode, INODE_FUSE_LOOKUP_REFERENCE, 1);
		}

		// This matches the lookup *we* did
		BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE, 1);
	}

}

static void *background_forget_worker(void *Context)
{
	list_entry_t *le = NULL;
	background_forget_work_t *work = NULL;

	(void) Context;

	pthread_mutex_lock(&background_cleanup_queue_lock);
	while (0 == background_thread_shutdown) {

		while (empty_list(&background_cleanup_queue)) {
			pthread_cond_wait(&background_cleanup_queue_cond, &background_cleanup_queue_lock);
			if (background_thread_shutdown) {
				break;
			}
		}

		if (background_thread_shutdown) {
			break;
		}

		le = remove_list_head(&background_cleanup_queue);
		work = container_of(le, background_forget_work_t, ListEntry);

		for (unsigned index = 0; index < work->Count; index++) {
			background_forget(work->BBud, 
							  work->ThingsToForget[index].InodeToForget,
							  work->ThingsToForget[index].Bias);
		}

		free(work);
		work = NULL;
		le = NULL;
		
	}
	pthread_mutex_unlock(&background_cleanup_queue_lock);

	pthread_exit(NULL);
}

static void background_forget_startup(void)
{
	pthread_mutex_lock(&background_cleanup_queue_lock);
	if (0 == background_thread_started) {
		int status = 0;
		pthread_attr_t attr;

		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		status = pthread_create(&background_thread, &attr, background_forget_worker, NULL);
		assert(0 == status);
		pthread_attr_destroy(&attr);
		background_thread_started = 1;
	}
	pthread_mutex_unlock(&background_cleanup_queue_lock);
}



void bitbucket_forget(fuse_req_t req, fuse_ino_t ino, uint64_t nlookup)
{
	struct timespec start, stop, elapsed;
	int status, tstatus;

	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	assert(0 == tstatus);
	status = bitbucket_internal_forget(req, ino, nlookup);
	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
	assert(0 == tstatus);
	timespec_diff(&start, &stop, &elapsed);
	BitbucketCountCall(BITBUCKET_CALL_FORGET, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_forget(fuse_req_t req, fuse_ino_t ino, uint64_t nlookup)
{
	void *userdata = fuse_req_userdata(req);
	bitbucket_userdata_t *BBud = (bitbucket_userdata_t *)userdata;
	background_forget_work_t *work = NULL;

	// TODO: replace this with the background startup stuff

	while (1) {

		if (0 == BBud->BackgroundForget) {
			background_forget(BBud, ino, nlookup);
			break;
		}

		if (0 == background_thread_started) {
			background_forget_startup();
		}

		work = (background_forget_work_t *)malloc(sizeof(background_forget_work_t));
		if (NULL == work) {
			background_forget(BBud, ino, nlookup);
			break;
		}

		work->BBud = BBud;
		work->Count = 1;
		work->ThingsToForget[0].InodeToForget = ino;
		work->ThingsToForget[0].Bias = nlookup;

		pthread_mutex_lock(&background_cleanup_queue_lock);
		insert_list_tail(&background_cleanup_queue, &work->ListEntry);
		pthread_cond_signal(&background_cleanup_queue_cond);
		pthread_mutex_unlock(&background_cleanup_queue_lock);
		break;

	}

	fuse_reply_err(req, 0);

	return 0;
}

static int bitbucket_internal_forget_multi(fuse_req_t req, size_t count, struct fuse_forget_data *forgets);


void bitbucket_forget_multi(fuse_req_t req, size_t count, struct fuse_forget_data *forgets)
{
	struct timespec start, stop, elapsed;
	int status, tstatus;

	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	assert(0 == tstatus);
	status = bitbucket_internal_forget_multi(req, count, forgets);
	tstatus = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
	assert(0 == tstatus);
	timespec_diff(&start, &stop, &elapsed);
	BitbucketCountCall(BITBUCKET_CALL_FORGET_MULTI, status ? 0 : 1, &elapsed);
}

static int bitbucket_internal_forget_multi(fuse_req_t req, size_t count, struct fuse_forget_data *forgets)
{
	void *userdata = fuse_req_userdata(req);
	bitbucket_userdata_t *BBud = (bitbucket_userdata_t *)userdata;
	struct fuse_forget_data *forget_array = forgets;
	background_forget_work_t *work = NULL;

	while (1) {

		if (1 == BBud->BackgroundForget) {
			if (0 == background_thread_started) {
				background_forget_startup();
				assert(1 == background_thread_started);
			}

			if (count <= BACKGROUND_FORGET_COUNT_MAX) {
				work = (background_forget_work_t *)malloc(sizeof(background_forget_work_t));
			}
		}

		// This is the "no posting" path:
		//  - If background forget is not enabled
		//  - If the malloc failed
		//  - If the number of forget items is larger than we've anticipated
		//
		if (NULL == work) {
			for (unsigned index = 0; index < count; index++) {
				background_forget(BBud, forget_array[index].ino, forget_array[index].nlookup);
			}
			break;
		}

		work->BBud = BBud;
		work->Count = count;
		assert(count <= BACKGROUND_FORGET_COUNT_MAX);
		for (unsigned index = 0; index < count; index++) {
			work->ThingsToForget[index].InodeToForget = forget_array[index].ino;
			work->ThingsToForget[index].Bias = forget_array[index].nlookup;
		}

		pthread_mutex_lock(&background_cleanup_queue_lock);
		insert_list_tail(&background_cleanup_queue, &work->ListEntry);
		pthread_cond_signal(&background_cleanup_queue_cond);
		pthread_mutex_unlock(&background_cleanup_queue_lock);
		break;

	}

	fuse_reply_err(req, 0);

	return 0;

}
