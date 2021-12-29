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

#include <stdint.h>
#include "php.h"
#include "php_memprof.h"
#include "profile.h"
#include "dump_callgrind.h"
#include "util.h"

static zend_bool dump_frame_callgrind(php_stream * stream, frame * f, char * fname, size_t * inclusive_size, size_t * inclusive_count)
{
	size_t size = 0;
	size_t count = 0;
	size_t self_size = 0;
	size_t self_count = 0;
	alloc * alloc;
	HashPosition pos;
	zval * znext;

	zend_hash_internal_pointer_reset_ex(&f->next_cache, &pos);
	while ((znext = zend_hash_get_current_data_ex(&f->next_cache, &pos)) != NULL) {
		zend_string * str_key;
		zend_ulong num_key;
		size_t call_size;
		size_t call_count;
		frame * next = Z_PTR_P(znext);

		if (HASH_KEY_IS_STRING != zend_hash_get_current_key_ex(&f->next_cache, &str_key, &num_key, &pos)) {
			continue;
		}

		if (!dump_frame_callgrind(stream, next, ZSTR_VAL(str_key), &call_size, &call_count)) {
			return 0;
		}

		size += call_size;
		count += call_count;

		zend_hash_move_forward_ex(&f->next_cache, &pos);
	}

	if (
		!stream_printf(stream, "fl=/todo.php\n") ||
		!stream_printf(stream, "fn=%s\n", fname)
	) {
		return 0;
	}

	LIST_FOREACH(alloc, &f->allocs, list) {
		self_size += alloc->size;
		self_count ++;
	}
	size += self_size;
	count += self_count;

	if (!stream_printf(stream, "1 %zu %zu\n", self_size, self_count)) {
		return 0;
	}

	zend_hash_internal_pointer_reset_ex(&f->next_cache, &pos);
	while ((znext = zend_hash_get_current_data_ex(&f->next_cache, &pos)) != NULL) {
		zend_string * str_key;
		zend_ulong num_key;
		size_t call_size;
		size_t call_count;
		frame * next = Z_PTR_P(znext);

		if (HASH_KEY_IS_STRING != zend_hash_get_current_key_ex(&f->next_cache, &str_key, &num_key, &pos)) {
			continue;
		}

		frame_inclusive_cost(next, &call_size, &call_count);

		if (
			!stream_printf(stream, "cfl=/todo.php\n")						||
			!stream_printf(stream, "cfn=%s\n", ZSTR_VAL(str_key))			||
			!stream_printf(stream, "calls=%zu 1\n", next->calls)			||
			!stream_printf(stream, "1 %zu %zu\n", call_size, call_count)
		) {
			return 0;
		}

		zend_hash_move_forward_ex(&f->next_cache, &pos);
	}

	if (!stream_printf(stream, "\n")) {
		return 0;
	}

	if (inclusive_size) {
		*inclusive_size = size;
	}
	if (inclusive_count) {
		*inclusive_count = count;
	}

	return 1;
}

zend_bool memprof_dump_callgrind(php_stream * stream, frame * root_frame) {
	size_t total_size;
	size_t total_count;

	return (
		stream_printf(stream, "version: 1\n")						&&
		stream_printf(stream, "cmd: unknown\n")						&&
		stream_printf(stream, "positions: line\n")					&&
		stream_printf(stream, "events: MemorySize BlocksCount\n")	&&
		stream_printf(stream, "\n")									&&

		dump_frame_callgrind(stream, root_frame, "root", &total_size, &total_count) &&

		stream_printf(stream, "total: %zu %zu\n", total_size, total_count)
	);
}
