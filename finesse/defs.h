/*
 * Copyright (c) 2016, Jake Wires. All rights reserved.
 */

#ifndef DEFS_H
#define DEFS_H

#include <stddef.h>
#include <stdint.h>

#ifndef typeof
#define typeof(_x) __typeof__(_x)
#endif

#ifndef offsetof
#define offsetof(_struct, _field) __builtin_offsetof(_struct, _field)
#endif

#ifndef containerof
#define containerof(_ptr, _struct, _field)                              \
    (typeof (_struct *))(                                               \
        (intptr_t)(_ptr) - offsetof(typeof(_struct), _field))
#endif

#define __packed __attribute__((packed))

#define __notused __attribute__((unused))

#define ARRAY_SIZE(_a) (sizeof(_a) / sizeof((_a)[0]))

#endif

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
