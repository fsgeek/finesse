//
// (C) Copyright 2020
// Tony Mason
// All Rights Reserved

#include "bitbucket.h"
#include <errno.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <fuse_kernel.h>
#pragma GCC diagnostic pop
#include <stdlib.h>
#include <string.h>

#if 0
// TODO: once you figure out what the code should look like, pull it into here
// and generalize to the readdir case as well.
static size_t readdirplus_add_entry(void)
{
	return 0;
}
#endif //0

void bitbucket_readdirplus(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi)
{
	void *userdata = fuse_req_userdata(req);
	bitbucket_user_data_t *BBud = (bitbucket_user_data_t *)userdata;
	bitbucket_inode_t *inode = NULL;
	bitbucket_dir_enum_context_t dirEnumContext;
	char *responseBuffer = NULL;
	size_t used = 0;
	size_t entrySize = 0;
	int status = 0;
	struct fuse_entry_param fep;

	(void) fi;

	// TODO: should we be doing anything with the flags in fi->flags?

	if (~0 == off) {
		// return the empty buffer
		fuse_reply_buf(req, NULL, 0);
		return;
	}

	CHECK_BITBUCKET_USER_DATA_MAGIC(BBud);

	if (FUSE_ROOT_ID == ino) {
		inode = BBud->RootDirectory;
		BitbucketReferenceInode(inode, INODE_LOOKUP_REFERENCE);
	}
	else {
		inode = BitbucketLookupInodeInTable(BBud->InodeTable, ino);
	}

	if (NULL == inode) {
		status = ENOENT;
	}

	BitbucketLockInode(inode, 0); // lock the directory for enumeration (shared)
	while (NULL != inode){
		const bitbucket_dir_entry_t *dirEntry = NULL;

		if (!S_ISDIR(inode->Attributes.st_mode)) {
			// This inode is not a directory inode
			status = ENOTDIR;
			break;
		}

		responseBuffer = malloc(size);
		if (NULL == responseBuffer) {
			status = ENOMEM;
			break;
		}

		BitbucketInitalizeDirectoryEnumerationContext(&dirEnumContext, inode);

		if (0 != off) {
			status = BitbucketSeekDirectory(&dirEnumContext, off);
			if (0 != status) {
				break;
			}
		}

		// As long as we might be able to fit another entry, we keep trying
		// This loop will terminate if:
		//   * We have no more entries
		//   * We don't have space to add more entries
		//   * The context structure indicates the directory has changed
		//
		while ((used + FUSE_NAME_OFFSET_DIRENTPLUS) < size) {

			dirEntry = BitbucketEnumerateDirectory(&dirEnumContext);

			if (NULL == dirEntry) {
				// End of the enumeration
				if (0 == used) {
					// In this case we return an error
					status = ENOENT;
				}
				else {
					// we've already packed in some entries, so we have to return them
					status = 0;
				}
				break;
			}

			if (ERESTART == dirEnumContext.LastError) {
				// Directory changed between calls
				assert(0 == used); // we can't have used any yet.
				status = ERESTART;
				break;
			}

			// Let's try to pack an entry into this buffer
			memset(&fep, 0, sizeof(fep));
			fep.attr = dirEntry->Inode->Attributes;
			fep.attr.st_mode = fep.attr.st_mode << 12; // not quite sure why fuse_add_direntry_plus expects this...
			fep.attr_timeout = 30.0; // TODO: this needs to be parameterized somewhere/somehow.
			fep.entry_timeout = 30.0; // Ditto...
			fep.generation = dirEntry->Inode->Epoch;
			fep.ino = dirEntry->Inode->Attributes.st_ino;
			entrySize = fuse_add_direntry_plus(	req,  // FUSE request
												responseBuffer + used, // buffer into which we are packing entries
												size - used,  // space remaining in the buffer
												dirEntry->Name, // name of this object
												&fep, 		   // fuse entry param structure
												dirEnumContext.Offset); // directory offset

			if (entrySize > size - used) {
				// This entry does not fit.  We can terminate here.
				// If this is the first entry, this will send a zero
				// length response buffer without an error.  It really
				// seems odd, but that's what appears to be expected.
				break;
			}

			// This call adds a reference to the inode - but not for . or ..
			if ((BITBUCKET_DIR_TYPE != dirEntry->Inode->InodeType) || // non-directories get referenced
			    ((inode != dirEntry->Inode) &&  // this would be '.'
				 (inode->Instance.Directory.Parent != dirEntry->Inode))) { // this would be '..'
				BitbucketReferenceInode(dirEntry->Inode, INODE_FUSE_REFERENCE);
			}

			fprintf(stderr, "Finesse (%s): added entry for %s with inode 0x%lx\n", __func__, dirEntry->Name, dirEntry->Inode->Attributes.st_ino);

			used += entrySize;
		}

		// Done at this point
		break;
	}
	BitbucketUnlockInode(inode); // don't need to keep the directory locked any longer

	if (NULL != inode) {
		// release our reference on the directory
		BitbucketDereferenceInode(inode, INODE_LOOKUP_REFERENCE);
	}
	
	if (0 != status) {
		fuse_reply_err(req, status);
	}
	else {
		assert(NULL != responseBuffer);
		fuse_reply_buf(req, responseBuffer, used);
	}

	// cleanup the response buffer
	free(responseBuffer);
	responseBuffer = NULL;

}
