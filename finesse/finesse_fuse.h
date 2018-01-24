#ifndef __FINESSE_FUSE_H__
#define __FINESSE_FUSE_H__

extern int finesse_send_reply_iov(fuse_req_t req, int error, struct iovec *iov, int count, int free_req);
extern void finesse_notify_reply_iov(fuse_req_t req, int error, struct iovec *iov, int count);

#endif // __FINESSE_FUSE_H__
