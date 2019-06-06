/*
 * Copyright (c) 2016, Jake Wires. All rights reserved.
 */

#ifndef LIST_H
#define LIST_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "defs.h"

struct list {
    struct list *prv;
    struct list *nxt;
};

#define LIST_INITIALIZER(_list)                 \
    (struct list) {                             \
        .prv = &(_list),                        \
        .nxt = &(_list),                        \
    }

#define LIST_ITEM_INITIALIZER(_item)            \
    (struct list) {                             \
        .prv = NULL,                            \
        .nxt = NULL,                            \
    }

static inline void
list_initialize(
    struct list *list)
{
    list->prv = list;
    list->nxt = list;
}

static inline void
list_item_initialize(
    struct list *item)
{
    item->nxt = NULL;
    item->prv = NULL;
}

static inline bool
list_is_empty(
    const struct list *list)
{
    return list->nxt == list;
}

static inline bool
list_item_is_linked(
    const struct list *item)
{
    return item->nxt != NULL;
}

static inline struct list *
list_head(
    struct list *list)
{
    return list->nxt;
}

static inline struct list *
list_tail(
    struct list *list)
{
    return list->prv;
}

static inline void
list_insert_before(
    struct list *list,
    struct list *item)
{
    item->nxt = list;
    item->prv = list->prv;
    list->prv->nxt = item;
    list->prv = item;
}

static inline void
list_insert_after(
    struct list *list,
    struct list *item)
{
    list_insert_before(list->nxt, item);
}

static inline void
list_insert_head(
    struct list *list,
    struct list *item)
{
    list_insert_after(list, item);
}

static inline void
list_insert_tail(
    struct list *list,
    struct list *item)
{
    list_insert_before(list, item);
}

static inline void
list_append(
    struct list *head,
    struct list *tail)
{
    if (!list_is_empty(tail)) {
        head->prv->nxt = tail->nxt;
        tail->nxt->prv = head->prv;
        head->prv = tail->prv;
        tail->prv->nxt = head;
        list_initialize(tail);
    }
}

static inline void
list_remove(
    struct list *item)
{
    item->prv->nxt = item->nxt;
    item->nxt->prv = item->prv;
    list_item_initialize(item);
}

#define list_item(_item, _type, _field)                                 \
    ((_type *)containerof(_item, _type, _field))

#define list_for_each(_list, _itr)                                      \
    for ((_itr) = list_head((_list));                                   \
         (_itr) != (_list);                                             \
         (_itr) = (_itr)->nxt)

#define list_for_each_reversed(_list, _itr)                             \
    for ((_itr) = list_tail((_list));                                   \
         (_itr) != (_list);                                             \
         (_itr) = (_itr)->prv)

#define list_for_each_item(_list, _ent, _field)                         \
    for ((_ent) =                                                       \
             list_item(                                                 \
                 list_head((_list)), typeof(*(_ent)), _field);          \
         (_ent) !=                                                      \
             list_item((_list), typeof(*(_ent)), _field);               \
         (_ent) =                                                       \
             list_item((_ent)->_field.nxt, typeof(*(_ent)), _field))

#define list_for_each_item_reversed(_list, _ent, _field)                \
    for ((_ent) =                                                       \
             list_item(                                                 \
                 list_tail((_list)), typeof(*(_ent)), _field);          \
         (_ent) !=                                                      \
             list_item((_list), typeof(*(_ent)), _field);               \
         (_ent) =                                                       \
             list_item((_ent)->_field.prv, typeof(*(_ent)), _field))

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
