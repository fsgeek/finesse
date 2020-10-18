/*
 * Copyright (c) 2020, Tony Mason. All rights reserved.
 */

enum test_dir_op_type {
    TEST_DIR_OP_TYPE_STAT,
    TEST_DIR_OP_TYPE_MKDIR,
    TEST_DIR_OP_TYPE_OPEN,
    TEST_DIR_OP_TYPE_CREATE,
    TEST_DIR_OP_TYPE_UNLINK,
    TEST_DIR_OP_TYPE_CLOSE,
    TEST_DIR_OP_TYPE_END,  // end of the operation vector
};

typedef struct _testbbfin_info {
    enum test_dir_op_type Operation;
    int                   ExpectedStatus;
    const char*           Pathname;
} testbbfin_info_t;

extern testbbfin_info_t test_dir_data[];
