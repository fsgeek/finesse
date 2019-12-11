/*
 * (C) Copyright 2017 Tony Mason
 * All Rights Reserved
*/

#if !defined(__FINESSE_MSG_H__)
#define __FINESSE_MSG_H__ (1)

// These are the messages used by finesse for shared memory.  These should probably be replaced at some
// point with something more efficient.
//

//
// we need to know what kind of message is being sent.
//
typedef enum {
	FINESSE_TEST = 42, // a random test value
	FINESSE_TEST_RESPONSE,

	FINESSE_NAME_MAP_REQUEST = 61, // map the name to a usable identifier
	FINESSE_NAME_MAP_RESPONSE,	 // respond to the name map request
	FINESSE_MAP_RELEASE_REQUEST,   // release a name map (handle)
	FINESSE_MAP_RELEASE_RESPONSE,  // respond to the release request
	FINESSE_DIR_MAP_REQUEST,	   // request a mapping of the directory contents
	FINESSE_DIR_MAP_RESPONSE,	  // response to dir map request
	FINESSE_UNLINK_REQUEST,		   // request file unlink
	FINESSE_UNLINK_RESPONSE,	   // respond to file unlink request
	FINESSE_STATFS_REQUEST,		  // request statfs
	FINESSE_STATFS_RESPONSE,         // respond to statfs request
	FINESSE_FSTATFS_REQUEST,		  // request fstatfs
	FINESSE_FSTATFS_RESPONSE,         // respond to fstatfs request

	FINESSE_PATH_SEARCH_REQUEST,  // look for a list of files in a list of paths
	FINESSE_PATH_SEARCH_RESPONSE, // respond to a search request

	// Everything beyond this is TODO
	FINESSE_FUSE_OP_REQUEST,
	FINESSE_FUSE_OP_RESPONSE,
	FINESSE_FUSE_NOTIFY, // FUSE notify
} finesse_message_type_t;

//
// This is the generic finesse key (identifier)
//
typedef struct finesse_key
{
	u_int8_t KeyLength;
	unsigned char Key[1];
} finesse_key_t;

struct finesse_uuid_t
{
	uint32_t data1;
	uint16_t data2;
	uint16_t data3;
	u_char data4[8];
};
typedef struct finesse_uuid_t finesse_uuid_t;

typedef struct
{
	u_int8_t MessageLength;
	u_char Message[1];
} finesse_test_message_t;

//
// This is the generic finesse message
//
#define FINESSE_MESSAGE_MAGIC "FINESSE"
#define FINESSE_MESSAGE_MAGIC_SIZE (8)

typedef struct finesse_message
{
	char MagicNumber[FINESSE_MESSAGE_MAGIC_SIZE];
	finesse_uuid_t SenderUuid;
	finesse_message_type_t MessageType;
	u_int64_t MessageId;
	u_int32_t MessageLength;
	char Message[1];
} finesse_message_t;

//
// This is the name being requested
//
typedef struct finesse_name_map_request
{
	u_int16_t NameLength;
	unsigned char Name[1];
} finesse_name_map_request_t;

//
// This is the name response
//
typedef struct finesse_name_map_response
{
	u_int32_t Status;
	finesse_key_t Key;
} finesse_name_map_response_t;

#define FINESSE_MAP_RESPONSE_SUCCESS 0
#define FINESSE_MAP_RESPONSE_NOTFOUND 2
#define FINESSE_MAP_RESPONSE_INVALID 22

typedef finesse_uuid_t finesse_buf_t;

typedef struct finesse_map_release_request
{
	finesse_key_t Key;
} finesse_map_release_request_t;

typedef struct finesse_map_release_response
{
	u_int32_t Status;
} finesse_map_release_response_t;

//
// Directory map logic
//
typedef struct finesse_dir_map_request
{
	finesse_key_t Key;
} finesse_dir_map_reqeust_t;

typedef struct finesse_dir_map_response
{
	finesse_uuid_t MapUuid;
} finesse_dir_map_response_t;

//
// Unlink structures
//
typedef struct finesse_unlink_requst
{
	u_int16_t NameLength;
	char Name[1];
} finesse_unlink_request_t;

typedef struct finesse_unlink_response
{
	u_int32_t Status;
} finesse_unlink_response_t;

//
// Statfs structures
//
typedef struct finesse_statfs_request
{
	u_int16_t PathLength;
	char Path[1]
} finesse_statfs_response_t;

typedef struct finesse_statfs_response
{
	u_int32_t Status;
	struct statvfs StatfsStruc;
} finesse_statfs_response_t;

//
// FStatfs structures
//
typedef struct finesse_fstatfs_request
{
	fuse_ino_t Nodeid;
} finesse_fstatfs_response_t;

typedef struct finesse_fstatfs_response
{
	u_int32_t Status;
	struct statvfs StatfsStruc;
} finesse_fstatfs_response_t;

//
// Path search structures
//
typedef struct finesse_path_search_request
{
	//
	// The search data consists of two lists:
	//
	// <string><null><string><null><string><null><null>
	// <string><null><string><null><null><null>
	//
	// TODO: we may need to have some way of specifying
	//       this is sent in shared memory.
	//
	u_int16_t SearchDataFlags;
	u_int16_t SearchDataLength;
	char SearchData[1];
} finesse_path_search_request_t;

#define FINESSE_SEARCH_DATA_RESIDENT (0x1)
#define FINESSE_SEARCH_DATA_SHARED_MEM (0x2)

typedef struct finesse_path_search_response
{
	u_int32_t Status;
	u_int16_t PathLength;

	// response is: <path>/<file>
	// it is invalid to have no / present
	char Path[1];
} finesse_path_search_response_t;

#if 0
struct fuse_buf {
	/**
	 * Size of data in bytes
	 */
	size_t size;

	/**
	 * Buffer flags
	 */
	enum fuse_buf_flags flags;

	/**
	 * Memory pointer
	 *
	 * Used unless FUSE_BUF_IS_FD flag is set.
	 */
	void *mem;

	/**
	 * File descriptor
	 *
	 * Used if FUSE_BUF_IS_FD flag is set.
	 */
	int fd;

	/**
	 * File position
	 *
	 * Used if FUSE_BUF_FD_SEEK flag is set.
	 */
	off_t pos;
};

#endif // 0

#endif // !defined(__FINESSE_MSG_H__)
