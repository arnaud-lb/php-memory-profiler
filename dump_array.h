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

#ifndef PHP_MEMPROF_DUMP_ARRAY_H
#define PHP_MEMPROF_DUMP_ARRAY_H

#include <stdint.h>
#include "php.h"
#include "php_memprof.h"
#include "profile.h"

zend_bool memprof_dump_array(zval * dest, frame * f);

#endif /* PHP_MEMPROF_DUMP_ARRAY_H */
