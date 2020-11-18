#!/bin/sh

set -xve

if ! grep -q memprof.so ~/.phpenv/versions/$(phpenv version-name)/etc/php.ini; then
    echo "extension = $(pwd)/modules/memprof.so" >> ~/.phpenv/versions/$(phpenv version-name)/etc/php.ini
fi
phpenv config-rm xdebug.ini || true

phpize
CFLAGS=$MEMPROF_CFLAGS ./configure
make
