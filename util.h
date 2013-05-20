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

void stream_printf(php_stream * stream, const char * format, ...);
void stream_write_word(php_stream * stream, zend_uintptr_t word);

size_t get_function_name(zend_execute_data * execute_data, char * buf, size_t buf_size);

