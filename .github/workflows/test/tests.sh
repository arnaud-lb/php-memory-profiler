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

# Hack to ensure that run-tests.php attempts to load xdebug as a zend_extension
# This hack is necessary with PHP < 8.1
sed -i "s/if (\$req_ext == 'opcache') {/if (\$req_ext == 'opcache' || \$req_ext == 'xdebug') {/" run-tests.php || true

PHP=$(which php)
REPORT_EXIT_STATUS=1 TEST_PHP_EXECUTABLE="$PHP" "$PHP" run-tests.php -q $checkmem --show-diff $showmem
