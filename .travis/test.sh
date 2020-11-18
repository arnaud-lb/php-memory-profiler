#!/bin/sh

set -xve

export PATH=$TRAVIS_BUILD_DIR/.travis:$PATH

PHP=$(which php)
REPORT_EXIT_STATUS=1 TEST_PHP_EXECUTABLE="$PHP" "$PHP" run-tests.php -s -q --show-diff
