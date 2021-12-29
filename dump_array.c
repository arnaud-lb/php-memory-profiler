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
#include "dump_array.h"

zend_bool memprof_dump_array(zval * dest, frame * f)
{
	HashPosition pos;
	zval * znext;
	zval * zframe = dest;
	zval zcalled_functions;
	alloc * alloc;
	size_t alloc_size = 0;
	size_t alloc_count = 0;
	size_t inclusive_size;
	size_t inclusive_count;

	array_init(zframe);

	LIST_FOREACH(alloc, &f->allocs, list) {
		alloc_size += alloc->size;
		alloc_count ++;
	}

	add_assoc_long_ex(zframe, ZEND_STRL("memory_size"), alloc_size);
	add_assoc_long_ex(zframe, ZEND_STRL("blocks_count"), alloc_count);

	frame_inclusive_cost(f, &inclusive_size, &inclusive_count);
	add_assoc_long_ex(zframe, ZEND_STRL("memory_size_inclusive"), inclusive_size);
	add_assoc_long_ex(zframe, ZEND_STRL("blocks_count_inclusive"), inclusive_count);

	add_assoc_long_ex(zframe, ZEND_STRL("calls"), f->calls);

	array_init(&zcalled_functions);

	zend_hash_internal_pointer_reset_ex(&f->callees, &pos);
	while ((znext = zend_hash_get_current_data_ex(&f->callees, &pos)) != NULL) {

		zend_string * str_key;
		zend_ulong num_key;
		zval zcalled_function;
		frame * next = Z_PTR_P(znext);

		if (HASH_KEY_IS_STRING != zend_hash_get_current_key_ex(&f->callees, &str_key, &num_key, &pos)) {
			continue;
		}

		memprof_dump_array(&zcalled_function, next);
		add_assoc_zval_ex(&zcalled_functions, ZSTR_VAL(str_key), ZSTR_LEN(str_key), &zcalled_function);

		zend_hash_move_forward_ex(&f->callees, &pos);
	}

	add_assoc_zval_ex(zframe, ZEND_STRL("called_functions"), &zcalled_functions);

	return 1;
}
