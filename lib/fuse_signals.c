/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  Utility functions for setting signal handlers.

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB
*/

#include "config.h"
#include "fuse_lowlevel.h"
#include "fuse_i.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

static struct fuse_session *fuse_instance;

static void exit_handler(int sig)
{
	if (fuse_instance) {
		fuse_session_exit(fuse_instance);
		if(sig <= 0) {
			fuse_log(FUSE_LOG_ERR, "assertion error: signal value <= 0\n");
			abort();
		}
		fuse_instance->error = sig;
	}
}

// Begin StackFS instrumentation
static void stats_handler(int sig)
{
        (void) sig;
        int /*fd,*/ i, j;
        FILE *fp = NULL;
        struct fuse_session *se;
        char *statsDir = NULL, *statsFile = NULL;

        se = fuse_instance;
        statsDir = fuse_session_statsDir(se);
        if (!statsDir) {
                printf("No Stats Directory to copy the statistics\n");
                return ;
        }
        statsFile = (char *)malloc(4096 * sizeof(char));
        statsFile[0] = '\0';
        strcpy(statsFile, statsDir);
        strcat(statsFile, "/user_stats.txt");

        pthread_spin_lock(&se->array_lock);
        fp = fopen(statsFile , "w" );
        if (fp) {
                for (i = 1; i < 46; i++) {
                        for (j = 0; j < 33; j++)
                                fprintf(fp, "%llu ", se->processing[i][j]);
                        fprintf(fp, "\n");
                }
        } else {
                perror("Failed to open User Stats File");
        }
        if (statsFile)
                free(statsFile);
        if (fp)
                fclose(fp);
        /* print the argument values to screen (remove soon after) */
        printf("Print mount options\n");
        //printf("allow_root : %d\n", se->f->allow_root);
        printf("max_write : %u\n", se->conn.max_write);
        printf("max_readahead : %u\n", se->conn.max_readahead);
        printf("max_background : %u\n", se->conn.max_background);
        printf("congestion_threshold : %u\n", se->conn.congestion_threshold);
        printf("async_read : %u\n", se->conn.want & FUSE_CAP_ASYNC_READ);
        printf("sync_read : %u\n", ~(se->conn.want & ~FUSE_CAP_ASYNC_READ));
        printf("atomic_o_trunc : %d\n", se->conn.want & FUSE_CAP_ATOMIC_O_TRUNC);
        printf("no_remote_lock : %d\n", (se->conn.want & FUSE_CAP_FLOCK_LOCKS) & (se->conn.want & FUSE_CAP_POSIX_LOCKS));
        printf("no_remote_flock : %d\n", se->conn.want & FUSE_CAP_FLOCK_LOCKS);
        printf("no_remote_posix_lock : %d\n", se->conn.want & FUSE_CAP_POSIX_LOCKS);
        //printf("big_writes : %d\n", se->f->big_writes);
        printf("splice_write : %d\n", se->conn.want & FUSE_CAP_SPLICE_WRITE);
        printf("splice_move : %d\n", se->conn.want & FUSE_CAP_SPLICE_MOVE);
        printf("splice_read : %d\n", se->conn.want & FUSE_CAP_SPLICE_READ);
        //printf("no_splice_move : %d\n", se->f->no_splice_move);
        //printf("no_splice_read : %d\n", se->f->no_splice_read);
        //printf("no_splice_write : %d\n", se->f->no_splice_write);
        printf("auto_inval_data : %d\n", se->conn.want & FUSE_CAP_AUTO_INVAL_DATA);
        //printf("no_auto_inval_data : %d\n", se->f->no_auto_inval_data);
        printf("no_readdirplus : %d\n", se->conn.want & ~FUSE_CAP_READDIRPLUS);
        printf("no_readdirplus_auto : %d\n", se->conn.want & ~FUSE_CAP_READDIRPLUS_AUTO);
        printf("async_dio : %d\n", se->conn.want & FUSE_CAP_ASYNC_DIO);
        //printf("no_async_dio : %d\n", se->f->no_async_dio);
        printf("writeback_cache : %d\n", se->conn.want & FUSE_CAP_WRITEBACK_CACHE);
        //printf("no_writeback_cache : %d\n", se->f->no_writeback_cache);
        printf("time_gran : %u\n", se->conn.time_gran);
        //printf("clone_fd : %d\n", se->f->clone_fd);

        pthread_spin_unlock(&se->array_lock);
}
//  End StackFS instrumentation


static void do_nothing(int sig)
{
	(void) sig;
}

static int set_one_signal_handler(int sig, void (*handler)(int), int remove)
{
	struct sigaction sa;
	struct sigaction old_sa;

	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = remove ? SIG_DFL : handler;
	sigemptyset(&(sa.sa_mask));
	sa.sa_flags = 0;

	if (sigaction(sig, NULL, &old_sa) == -1) {
		perror("fuse: cannot get old signal handler");
		return -1;
	}

	if (old_sa.sa_handler == (remove ? handler : SIG_DFL) &&
	    sigaction(sig, &sa, NULL) == -1) {
		perror("fuse: cannot set signal handler");
		return -1;
	}
	return 0;
}

int fuse_set_signal_handlers(struct fuse_session *se)
{
	/* If we used SIG_IGN instead of the do_nothing function,
	   then we would be unable to tell if we set SIG_IGN (and
	   thus should reset to SIG_DFL in fuse_remove_signal_handlers)
	   or if it was already set to SIG_IGN (and should be left
	   untouched. */
	if (set_one_signal_handler(SIGHUP, exit_handler, 0) == -1 ||
	    set_one_signal_handler(SIGINT, exit_handler, 0) == -1 ||
	    set_one_signal_handler(SIGTERM, exit_handler, 0) == -1 ||
	    set_one_signal_handler(SIGPIPE, do_nothing, 0) == -1 ||
            // Begin StackFS instrumentation
            set_one_signal_handler(SIGPIPE, SIG_IGN, 0) == -1 || 
	    set_one_signal_handler(SIGUSR1, stats_handler, 0) == -1 )
            // End StackFS instrumentation
		return -1;

	printf("Sucessfully registered signal Handlers : %d \n", getpid());
        fuse_instance = se;
	return 0;
}

void fuse_remove_signal_handlers(struct fuse_session *se)
{
	if (fuse_instance != se)
		fuse_log(FUSE_LOG_ERR,
			"fuse: fuse_remove_signal_handlers: unknown session\n");
	else
		fuse_instance = NULL;

	set_one_signal_handler(SIGHUP, exit_handler, 1);
	set_one_signal_handler(SIGINT, exit_handler, 1);
	set_one_signal_handler(SIGTERM, exit_handler, 1);
	set_one_signal_handler(SIGPIPE, do_nothing, 1);
        // Begin StackFS instrumentation
        set_one_signal_handler(SIGUSR1, stats_handler, 1);
        // End StackFS instrumentation
}
