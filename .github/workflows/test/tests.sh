#!/bin/sh

set -xve

export PATH="$HOME/php/bin:$PATH"
hash -r

cd ext

if [ "$MEMORY_CHECK" = "1" ]; then
      echo "Enabling memory checking"
      showmem=--show-mem
      checkmem=-m
fi

PHP=$(which php)
REPORT_EXIT_STATUS=1 TEST_PHP_EXECUTABLE="$PHP" "$PHP" run-tests.php -q $checkmem --show-diff $showmem
