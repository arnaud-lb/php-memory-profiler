/*
  +----------------------------------------------------------------------+
  | Memprof                                                              |
  +----------------------------------------------------------------------+
  | Copyright (c) 2021 Arnaud Le Blanc                                   |
  +----------------------------------------------------------------------+
  | Redistribution and use in source and binary forms, with or without   |
  | modification, are permitted provided that the conditions mentioned   |
  | in the accompanying LICENSE file are met.                            |
  +----------------------------------------------------------------------+
  | Author: Arnaud Le Blanc <arnaud.lb@gmail.com>                        |
  +----------------------------------------------------------------------+
*/

#ifndef PHP_MEMPROF_PROFILE_H
#define PHP_MEMPROF_PROFILE_H

#include "php.h"
#include <stdlib.h>
#include <sys/queue.h>

typedef LIST_HEAD(_alloc_list_head, _alloc) alloc_list_head;

/* a call frame */
typedef struct _frame {
	size_t calls;
	HashTable callees;
	alloc_list_head allocs;
} frame;

/* an allocated block's infos */
typedef struct _alloc {
#if MEMPROF_DEBUG
	size_t canary_a;
#endif
	LIST_ENTRY(_alloc) list;
	size_t size;
#if MEMPROF_DEBUG
	size_t canary_b;
#endif
} alloc;

typedef union _alloc_bucket_item {
	alloc alloc;
	union _alloc_bucket_item * next_free;
} alloc_bucket_item;

typedef struct _alloc_buckets {
	size_t growsize;
	size_t nbuckets;
	alloc_bucket_item * next_free;
	alloc_bucket_item ** buckets;
} alloc_buckets;

static inline size_t frame_alloc_size(const frame * f)
{
	size_t size = 0;
	alloc * alloc;

	LIST_FOREACH(alloc, &f->allocs, list) {
		size += alloc->size;
	}

	return size;
}

static inline void frame_inclusive_cost(frame * f, size_t * inclusive_size, size_t * inclusive_count)
{
	size_t size = 0;
	size_t count = 0;
	alloc * alloc;
	HashPosition pos;
	zval * znext;

	LIST_FOREACH(alloc, &f->allocs, list) {
		size += alloc->size;
		count ++;
	}

	zend_hash_internal_pointer_reset_ex(&f->callees, &pos);
	while ((znext = zend_hash_get_current_data_ex(&f->callees, &pos)) != NULL) {
		zend_string * str_key;
		zend_ulong num_key;
		size_t call_size;
		size_t call_count;
		frame * next = Z_PTR_P(znext);

		if (HASH_KEY_IS_STRING != zend_hash_get_current_key_ex(&f->callees, &str_key, &num_key, &pos)) {
			continue;
		}

		frame_inclusive_cost(next, &call_size, &call_count);

		size += call_size;
		count += call_count;

		zend_hash_move_forward_ex(&f->callees, &pos);
	}

	*inclusive_size = size;
	*inclusive_count = count;
}

#endif /* PHP_MEMPROF_PROFILE_H */
