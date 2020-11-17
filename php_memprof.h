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

#ifndef PHP_MEMPROF_H
#define PHP_MEMPROF_H

#define MEMPROF_NAME "memprof"
#define PHP_MEMPROF_VERSION "2.1.2"

extern zend_module_entry memprof_module_entry;
#define phpext_memprof_ptr &memprof_module_entry

#ifdef PHP_WIN32
#	define PHP_MEMPROF_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_MEMPROF_API __attribute__ ((visibility("default")))
#else
#	define PHP_MEMPROF_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

#ifndef PHP_FE_END
#   define PHP_FE_END { NULL, NULL, NULL, 0, 0 }
#endif

PHP_MINIT_FUNCTION(memprof);
PHP_MSHUTDOWN_FUNCTION(memprof);
PHP_RINIT_FUNCTION(memprof);
PHP_RSHUTDOWN_FUNCTION(memprof);
PHP_MINFO_FUNCTION(memprof);

PHP_FUNCTION(memprof_dump_callgrind);
PHP_FUNCTION(memprof_dump_pprof);
PHP_FUNCTION(memprof_dump_array);
PHP_FUNCTION(memprof_memory_get_usage);
PHP_FUNCTION(memprof_memory_get_peak_usage);
PHP_FUNCTION(memprof_enable);
PHP_FUNCTION(memprof_disable);
PHP_FUNCTION(memprof_enabled);
PHP_FUNCTION(memprof_enabled_flags);

#endif	/* PHP_MEMPROF_H */
