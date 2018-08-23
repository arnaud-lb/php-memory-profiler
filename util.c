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

void stream_printf(php_stream * stream, const char * format, TSRMLS_D, ...)
{
	char * buf;
	va_list ap;
	int len;

	va_start(ap, format);
	len = vspprintf(&buf, 0, format, ap);
	va_end(ap);

	php_stream_write(stream, buf, len );

	efree(buf);
}

void stream_write_word(php_stream * stream, zend_uintptr_t word TSRMLS_DC)
{
	php_stream_write(stream, (char*) &word, sizeof(word) );
}

size_t get_function_name(zend_execute_data * execute_data, char * buf, size_t buf_size TSRMLS_DC)
{
	const char * function_name = NULL;
	const char * call_type = NULL;
	char * free_class_name = NULL;
	const char * class_name = NULL;
	size_t len;

	if (!execute_data) {
		return snprintf(buf, buf_size, "main");
	}

	function_name = get_active_function_name(TSRMLS_C);

	if (function_name) {
		class_name = get_active_class_name(&call_type TSRMLS_CC);
	} else {
		if (!execute_data->opline || execute_data->opline->opcode != ZEND_INCLUDE_OR_EVAL) {
			function_name = "unknown";
		} else {
			switch (execute_data->opline->extended_value) {
				case ZEND_EVAL:
					function_name = "eval";
					break;
				case ZEND_INCLUDE:
					function_name = "include";
					break;
				case ZEND_REQUIRE:
					function_name = "require";
					break;
				case ZEND_INCLUDE_ONCE:
					function_name = "include_once";
					break;
				case ZEND_REQUIRE_ONCE:
					function_name = "require_once";
					break;
				default:
					function_name = "unknown";
					break;
			}
		}

		call_type = NULL;
	}

	len = snprintf(buf, buf_size, "%s%s%s", class_name != NULL ? class_name : "", call_type != NULL ? call_type : "", function_name != NULL ? function_name : "");

	if (free_class_name) {
		efree((char*)free_class_name);
	}

	return len >= buf_size ? buf_size-1 : len;
}

