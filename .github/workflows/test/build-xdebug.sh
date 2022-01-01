#!/bin/sh

set -e

export PATH="$HOME/php/bin:$PATH"
hash -r

git clone --depth 1 --branch "3.1.2" "https://github.com/xdebug/xdebug.git"

cd xdebug

case "$BUILD_ARCH" in
    i386)
        PHP_EXT_CFLAGS="$PHP_EXT_CFLAGS -m32"
        ;;
esac

phpize
CFLAGS="$PHP_EXT_CFLAGS" ./configure
make
sudo make install
