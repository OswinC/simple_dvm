/*
 *******************************************************************************
 * $Header:$
 *
 *  Copyright (c) 2000-2011 Vivotek Inc. All rights reserved.
 *
 *  +-----------------------------------------------------------------+
 *  | THIS SOFTWARE IS FURNISHED UNDER A LICENSE AND MAY ONLY BE USED |
 *  | AND COPIED IN ACCORDANCE WITH THE TERMS AND CONDITIONS OF SUCH  |
 *  | A LICENSE AND WITH THE INCLUSION OF THE THIS COPY RIGHT NOTICE. |
 *  | THIS SOFTWARE OR ANY OTHER COPIES OF THIS SOFTWARE MAY NOT BE   |
 *  | PROVIDED OR OTHERWISE MADE AVAILABLE TO ANY OTHER PERSON. THE   |
 *  | OWNERSHIP AND TITLE OF THIS SOFTWARE IS NOT TRANSFERRED.        |
 *  |                                                                 |
 *  | THE INFORMATION IN THIS SOFTWARE IS SUBJECT TO CHANGE WITHOUT   |
 *  | ANY PRIOR NOTICE AND SHOULD NOT BE CONSTRUED AS A COMMITMENT BY |
 *  | VIVOTEK INC.                                                    |
 *  +-----------------------------------------------------------------+
 *
 * $History:$
 * 
 *******************************************************************************
 */
/*!
 *******************************************************************************
 * Copyright 2000-2011 Vivotek, Inc. All rights reserved.
 *
 * \file
 * list_head.h
 *
 * \brief
 * The doubly linked list implementation
 *
 * \date
 * 2011/07/14
 *
 * \author
 * Ramax Lo
 *
 *******************************************************************************
 */
#ifndef _LIST_HEAD_H_
#define _LIST_HEAD_H_

struct list_head
{
	struct list_head *prev;
	struct list_head *next;
};

#define container_of(ptr, st, en) \
	({char *__p = (char *)&(((st *)0)->en); (st *)((char *)ptr - (unsigned long)__p);})

#define foreach(e, list) \
	for (e = (list)->next; e != (list); e = e->next)

#define foreach_safe(e, tmp, list) \
	for (e = (list)->next, tmp = e->next; e != (list); e = tmp, tmp = tmp->next)

#define LIST_INIT(x) \
	struct list_head x = {&x, &x}

static inline void list_init(struct list_head *entry)
{
	entry->next = entry;
	entry->prev = entry;
}

static inline struct list_head *list_next(struct list_head *entry)
{
	return entry->next;
}

static inline struct list_head *list_prev(struct list_head *entry)
{
	return entry->prev;
}

static inline int list_empty(struct list_head *entry)
{
	return (entry->next != entry) ? 0 : 1;
}

static inline void list_add(struct list_head *entry, struct list_head *list)
{
	entry->next = list->next;
	entry->prev = list;
	list->next->prev = entry;
	list->next = entry;
}

static inline void list_remote(struct list_head *entry)
{
	entry->prev->next = entry->next;
	entry->next->prev = entry->prev;
	entry->next = NULL;
	entry->prev = NULL;
}

static inline void list_add_tail(struct list_head *entry, struct list_head *list)
{
	list_add(entry, list->prev);
}

static inline void list_add_head(struct list_head *entry, struct list_head *list)
{
	list_add(entry, list);
}

#endif // _LIST_HEAD_H_

