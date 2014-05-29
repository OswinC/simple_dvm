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

struct TListHead
{
	struct TListHead *ptPrev;
	struct TListHead *ptNext;
};

#define container_of(ptr, st, en) \
	({char *p = (char *)&(((st *)0)->en); (st *)((char *)ptr - (unsigned long)p);})

#define foreach(e, list) \
	for (e = (list)->ptNext; e != (list); e = e->ptNext)

#define foreach_safe(e, tmp, list) \
	for (e = (list)->ptNext, tmp = e->ptNext; e != (list); e = tmp, tmp = tmp->ptNext)

#define LIST_INIT(x) \
	struct TListHead x = {&x, &x}

static inline void InitList(struct TListHead *ptEntry)
{
	ptEntry->ptNext = ptEntry;
	ptEntry->ptPrev = ptEntry;
}

static inline struct TListHead *ListNext(struct TListHead *ptEntry)
{
	return ptEntry->ptNext;
}

static inline struct TListHead *ListPrev(struct TListHead *ptEntry)
{
	return ptEntry->ptPrev;
}

static inline int ListIsEmpty(struct TListHead *ptEntry)
{
	return (ptEntry->ptNext != ptEntry) ? 0 : 1;
}

static inline void ListAdd(struct TListHead *ptEntry, struct TListHead *ptList)
{
	ptEntry->ptNext = ptList->ptNext;
	ptEntry->ptPrev = ptList;
	ptList->ptNext->ptPrev = ptEntry;
	ptList->ptNext = ptEntry;
}

static inline void ListRemove(struct TListHead *ptEntry)
{
	ptEntry->ptPrev->ptNext = ptEntry->ptNext;
	ptEntry->ptNext->ptPrev = ptEntry->ptPrev;
	ptEntry->ptNext = NULL;
	ptEntry->ptPrev = NULL;
}

static inline void ListAddTail(struct TListHead *ptEntry, struct TListHead *ptList)
{
	ListAdd(ptEntry, ptList->ptPrev);
}

static inline void ListAddHead(struct TListHead *ptEntry, struct TListHead *ptList)
{
	ListAdd(ptEntry, ptList);
}

#endif // _LIST_HEAD_H_

