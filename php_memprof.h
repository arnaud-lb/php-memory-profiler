/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2012 Arnaud Le Blanc                              |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Arnaud Le Blanc <arnaud.lb@gmail.com>                        |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_MEMPROF_H
#define PHP_MEMPROF_H

#define MEMPROF_NAME "memprof"
#define MEMPROF_VERSION "0.1"

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

/*
ZEND_BEGIN_MODULE_GLOBALS(memprof)
ZEND_END_MODULE_GLOBALS(memprof)
*/

#ifdef ZTS
#define MEMPROF_G(v) TSRMG(memprof_globals_id, zend_memprof_globals *, v)
#else
#define MEMPROF_G(v) (memprof_globals.v)
#endif

#endif	/* PHP_MEMPROF_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
