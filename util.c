/*
  +----------------------------------------------------------------------+
  | Memprof                                                              |
  +----------------------------------------------------------------------+
  | Copyright (c) 2012-2013 Arnaud Le Blanc                              |
  +----------------------------------------------------------------------+
  | Redistribution and use in source and binary forms, with or without   |
  | modification, are permitted provided that the conditions mentioned   |
  | in the accompanying LICENSE file are met.                            |
  +----------------------------------------------------------------------+
  | Author: Arnaud Le Blanc <arnaud.lb@gmail.com>                        |
  +----------------------------------------------------------------------+
*/

#include "php.h"
#include <stdarg.h>

void stream_printf(php_stream * stream, const char * format, ...)
{
	char * buf;
	va_list ap;
	int len;

	va_start(ap, format);
	len = vspprintf(&buf, 0, format, ap);
	va_end(ap);

	php_stream_write(stream, buf, len);

	efree(buf);
}

void stream_write_word(php_stream * stream, zend_uintptr_t word)
{
	php_stream_write(stream, (char*) &word, sizeof(word));
}

size_t get_function_name(zend_execute_data * execute_data, char * buf, size_t buf_size)
{
	const char * function_name = NULL;
	const char * call_type = NULL;
	const char * class_name = NULL;
	size_t len;
	zend_function * func;
	zend_string * zname;
	const char * include_type;
	const char * file_name;

	if (!execute_data) {
		return snprintf(buf, buf_size, "main");
	}

	func = EG(current_execute_data)->func;

	if (func->type != ZEND_USER_FUNCTION && func->type != ZEND_INTERNAL_FUNCTION) {
		return snprintf(buf, buf_size, "main");
	}

	if (execute_data->func == &zend_pass_function) {
		return snprintf(buf, buf_size, "zend_pass_function");
	}

	zname = func->common.function_name;

	if (zname == NULL) {
		zend_execute_data * include_execute_data = execute_data;
		if (include_execute_data->opline->opcode != ZEND_INCLUDE_OR_EVAL && include_execute_data->prev_execute_data != NULL && include_execute_data->prev_execute_data->opline->opcode == ZEND_INCLUDE_OR_EVAL) {
			include_execute_data = execute_data->prev_execute_data;
		}
		switch (include_execute_data->opline->extended_value) {
			case ZEND_EVAL:
				include_type = "eval";
				break;
			case ZEND_INCLUDE:
				include_type = "include";
				break;
			case ZEND_INCLUDE_ONCE:
				include_type = "include_once";
				break;
			case ZEND_REQUIRE:
				include_type = "require";
				break;
			case ZEND_REQUIRE_ONCE:
				include_type = "require_once";
				break;
			default:
				include_type = "main";
				break;
		}

		if (func->type == ZEND_USER_FUNCTION) {
			file_name = func->op_array.filename->val;
		} else {
			file_name = NULL;
		}

		len = snprintf(buf, buf_size, "%s %s", include_type, file_name);

		return len >= buf_size ? buf_size-1 : len;
	}

	function_name = ZSTR_VAL(zname);
	class_name = get_active_class_name(&call_type);

	len = snprintf(buf, buf_size, "%s%s%s", class_name != NULL ? class_name : "", call_type != NULL ? call_type : "", function_name != NULL ? function_name : "");

	return len >= buf_size ? buf_size-1 : len;
}

