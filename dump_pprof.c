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
#include "dump_pprof.h"
#include "util.h"

static zend_bool dump_frames_pprof(php_stream * stream, HashTable * symbols, frame * f)
{
	HashPosition pos;
	frame * prev;
	zval * znext;
	size_t size = frame_alloc_size(f);
	size_t stack_depth = frame_stack_depth(f);

	if (0 < size) {
		stream_write_word(stream, size);
		stream_write_word(stream, stack_depth);

		for (prev = f; prev->prev != prev; prev = prev->prev) {
			zend_uintptr_t symaddr;
			symaddr = (zend_uintptr_t) zend_hash_str_find_ptr(symbols, prev->name, prev->name_len);
			if (symaddr == 0) {
				/* shouldn't happen */
				zend_error(E_CORE_ERROR, "symbol address not found");
				return 0;
			}
			if (!stream_write_word(stream, symaddr)) {
				return 0;
			}
		}
	}

	zend_hash_internal_pointer_reset_ex(&f->next_cache, &pos);
	while ((znext = zend_hash_get_current_data_ex(&f->next_cache, &pos)) != NULL) {
		zend_string * str_key;
		zend_ulong num_key;
		frame * next = Z_PTR_P(znext);

		if (HASH_KEY_IS_STRING != zend_hash_get_current_key_ex(&f->next_cache, &str_key, &num_key, &pos)) {
			continue;
		}

		if (!dump_frames_pprof(stream, symbols, next)) {
			return 0;
		}

		zend_hash_move_forward_ex(&f->next_cache, &pos);
	}

	return 1;
}

static zend_bool dump_frames_pprof_symbols(php_stream * stream, HashTable * symbols, frame * f)
{
	HashPosition pos;
	zval * znext;
	zend_uintptr_t symaddr;

	if (!zend_hash_str_exists(symbols, f->name, f->name_len)) {
		/* addr only has to be unique */
		symaddr = (symbols->nNumOfElements+1)<<3;
		zend_hash_str_add_ptr(symbols, f->name, f->name_len, (void*) symaddr);
		if (!stream_printf(stream, "0x%0*x %s\n", sizeof(symaddr)*2, symaddr, f->name)) {
			return 0;
		}
	}

	zend_hash_internal_pointer_reset_ex(&f->next_cache, &pos);
	while ((znext = zend_hash_get_current_data_ex(&f->next_cache, &pos)) != NULL) {
		zend_string * str_key;
		zend_ulong num_key;
		frame * next = Z_PTR_P(znext);

		if (HASH_KEY_IS_STRING != zend_hash_get_current_key_ex(&f->next_cache, &str_key, &num_key, &pos)) {
			continue;
		}

		if (!dump_frames_pprof_symbols(stream, symbols, next)) {
			return 0;
		}

		zend_hash_move_forward_ex(&f->next_cache, &pos);
	}

	return 1;
}

static zend_bool dump_pprof_symbols_section(php_stream * stream, HashTable * symbols, frame * root_frame) {
	return (
		stream_printf(stream, "--- symbol\n")					&&
		stream_printf(stream, "binary=todo.php\n")				&&

		dump_frames_pprof_symbols(stream, symbols, root_frame)	&&

		stream_printf(stream, "---\n")
	);
}

static zend_bool dump_pprof_profile_section(php_stream * stream, HashTable * symbols, frame * root_frame) {
	return (
		stream_printf(stream, "--- profile\n") &&

		/* header count */
		stream_write_word(stream, 0)  &&

		/* header words after this one */
		stream_write_word(stream, 3)  &&

		/* format version */
		stream_write_word(stream, 0)  &&

		/* sampling period */
		stream_write_word(stream, 0)  &&

		/* unused padding */
		stream_write_word(stream, 0)  &&

		dump_frames_pprof(stream, symbols, root_frame)
	);
}

zend_bool memprof_dump_pprof(php_stream * stream, frame * root_frame) {
	HashTable symbols;

	zend_hash_init(&symbols, 8, NULL, NULL, 0);

	zend_bool success = (
		dump_pprof_symbols_section(stream, &symbols, root_frame) &&
		dump_pprof_profile_section(stream, &symbols, root_frame)
	);

	zend_hash_destroy(&symbols);

	return success;
}

