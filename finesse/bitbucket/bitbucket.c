//
// FUSE: Bitbucket
//
// (C) Copyright 2020 Tony Mason (fsgeek@cs.ubc.ca)
// All Rights Reserved
//

#define FUSE_USE_VERSION 39

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include "bitbucket.h"
#include <fuse_common.h>

#include <fuse_lowlevel.h>

static const struct fuse_lowlevel_ops bitbucket_ll_oper = {
    .init = bitbucket_init,
    .destroy = bitbucket_destroy,
    .lookup = bitbucket_lookup,
    .forget = bitbucket_forget,
    .getattr = bitbucket_getattr,
    .setattr = bitbucket_setattr,
    .readlink = bitbucket_readlink,
    .mknod = bitbucket_mknod,
    .mkdir = bitbucket_mkdir,
    .unlink = bitbucket_unlink,
    .rmdir = bitbucket_rmdir,
    .symlink = bitbucket_symlink,
    .rename = bitbucket_rename,
    .link = bitbucket_link,
    .open = bitbucket_open,
    .read = bitbucket_read,
    .write = bitbucket_write,
    .flush = bitbucket_flush,
    .release = bitbucket_release,
    .fsync = bitbucket_fsync,
    .opendir = bitbucket_opendir,
    .readdir = bitbucket_readdir,
    .releasedir = bitbucket_releasedir,
    .fsyncdir = bitbucket_fsyncdir,
    .statfs = bitbucket_statfs,
    .setxattr = bitbucket_setxattr,
    .getxattr = bitbucket_getxattr,
    .listxattr = bitbucket_listxattr,
    .removexattr = bitbucket_removexattr,
    .access = bitbucket_access,
    .create = bitbucket_create,
    .getlk = bitbucket_getlk,
    .setlk = bitbucket_setlk,
    .bmap = bitbucket_bmap,
    .ioctl = bitbucket_ioctl,
    .poll = bitbucket_poll,
    .write_buf = bitbucket_write_buf,
    .retrieve_reply = bitbucket_retrieve_reply,
    .forget_multi = bitbucket_forget_multi,
    .flock = bitbucket_flock,
    .fallocate = bitbucket_fallocate,
    .readdirplus = bitbucket_readdirplus,
    .copy_file_range = bitbucket_copy_file_range,
    .lseek = bitbucket_lseek,
};

int main(int argc, char *argv[])
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct fuse_session *se;
	struct fuse_cmdline_opts opts;
	int ret = -1;

	if (fuse_parse_cmdline(&args, &opts) != 0)
		return 1;
	if (opts.show_help) {
		printf("usage: %s [options] <mountpoint>\n\n", argv[0]);
		fuse_cmdline_help();
		fuse_lowlevel_help();
		ret = 0;
		goto err_out1;
	} else if (opts.show_version) {
		printf("FUSE library version %s\n", fuse_pkgversion());
		fuse_lowlevel_version();
		ret = 0;
		goto err_out1;
	}

	if(opts.mountpoint == NULL) {
		printf("usage: %s [options] <mountpoint>\n", argv[0]);
		printf("       %s --help\n", argv[0]);
		ret = 1;
		goto err_out1;
	}

	se = fuse_session_new(&args, &bitbucket_ll_oper,
			      sizeof(bitbucket_ll_oper), NULL);
	if (se == NULL)
	    goto err_out1;

	if (fuse_set_signal_handlers(se) != 0)
	    goto err_out2;

	if (fuse_session_mount(se, opts.mountpoint) != 0)
	    goto err_out3;

	fuse_daemonize(opts.foreground);

	/* Block until ctrl+c or fusermount -u */
	if (opts.singlethread)
		ret = fuse_session_loop(se);
	else
		ret = fuse_session_loop_mt(se, opts.clone_fd);

	fuse_session_unmount(se);
err_out3:
	fuse_remove_signal_handlers(se);
err_out2:
	fuse_session_destroy(se);
err_out1:
	free(opts.mountpoint);
	fuse_opt_free_args(&args);

	return ret ? 1 : 0;
}
