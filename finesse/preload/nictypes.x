%#include <stdbool.h>
%#include <inttypes.h>

/* Base types for nicfs */
typedef uint64_t nicfs_off_t;
typedef uint64_t nicfs_size_t;
typedef uint32_t nicfs_mode_t;

enum nicfs_key_type_t {
    NICFS_KEY_TYPE_INODE = 5,
    NICFS_KEY_TYPE_UUID = 6,
    NICFS_KEY_TYPE_OTHER = 7
};

struct nicfs_uuid_t {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    unsigned char data4[8];
};

typedef nicfs_uuid_t nicfs_key_uuid_t;

typedef uint64_t nicfs_key_inode_t;

struct nicfs_key_other_t {
    opaque key<>;
};

union nicfs_key_t switch(nicfs_key_type_t type) {
case NICFS_KEY_TYPE_INODE:
    nicfs_key_inode_t ino;
case NICFS_KEY_TYPE_UUID:
    nicfs_key_uuid_t uuid;
case NICFS_KEY_TYPE_OTHER:
    nicfs_key_other_t other;
};

/* errors returned from nicfs */
enum nicfs_err_t {
    NICFS_ok = 0,
    NICFS_err_perm = 1,

    /* these are NIFS specific */
    NICFS_use_long_path = 231
};


/* This list should include all the various I/O calls
   that might be used by an application. There is no
   requirement that we implement them all - this is
   more to show them so we can implement as needed. */
enum nicfs_request_t {
    NICFS_REQ_MAP_NAME,
    NICFS_REQ_FUSE_OP,
    /* unit test request(s) */
    NICFS_REQ_TEST_ECHO
};

struct nicfs_map_name_req_t {
    opaque fname<>;
};

struct nicfs_map_name_rsp_t {
    opaque map_name<>;
};


struct nicfs_buf_t {
    opaque url<>; /* this is the URL for the buffer */
    nicfs_off_t offset; /* this is the offset relative to the URL beginning */
    nicfs_size_t length; /* this is the length of the buffer */
};

struct nicfs_open_req_t {
    opaque fname<>;    /* name of the file */
    uint32_t flags;    /* open flags */
    nicfs_mode_t mode; /* open/creat mode bits */
};

struct nicfs_open_rsp_t {
    nicfs_key_t fkey;
};

struct nicfs_open_op_t {
    nicfs_open_req_t req;
    nicfs_open_rsp_t rsp;
};

struct nicfs_close_req_t {
    nicfs_key_t fkey;
};

struct nicfs_close_op_t {
    nicfs_close_req_t req;
};

struct nicfs_read_req_t {
    nicfs_key_t fkey;
    nicfs_off_t offset;
    nicfs_size_t length;
    nicfs_buf_t buffer;
};

struct nicfs_read_rsp_t {
    nicfs_size_t byes_read;
};

struct nicfs_read_op_t {
    nicfs_read_req_t req;
    nicfs_read_rsp_t rsp;
};

struct nicfs_echo_req_t {
    int test_in;
};

struct nicfs_echo_rsp_t {
    int test_out;
};

struct nicfs_echo_op_t {
    nicfs_echo_req_t req;
    nicfs_echo_rsp_t rsp;
};

struct nicfs_map_name_op_t {
    nicfs_map_name_req_t req;
    nicfs_map_name_rsp_t rsp;
};

struct nicfs_fuse_req_t {
    opaque fuse_req<>;
};

struct nicfs_fuse_rsp_t {
    opaque fuse_rsp<>;
};

struct nicfs_req_fuse_op_t {
    nicfs_fuse_req_t req;
    nicfs_fuse_rsp_t rsp;
};

union nicfs_op_t switch(nicfs_request_t type) {
case NICFS_REQ_MAP_NAME:
    nicfs_map_name_op_t obj_map_name;
case NICFS_REQ_FUSE_OP:
    nicfs_req_fuse_op_t obj_fuse;
};

