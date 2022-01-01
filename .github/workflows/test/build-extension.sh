#!/bin/sh

set -e

export PATH="$HOME/php/bin:$PATH"
hash -r

echo "Building extension with PHP version:"
php --version

if which apt-get >/dev/null 2>&1; then
    sudo apt-get -y install libjudy-dev:$BUILD_ARCH
else
    brew install traildb/judy/judy
fi

if [ "$WERROR" = "1" ]; then
    PHP_EXT_CFLAGS="-Wall -Werror -Wno-deprecated-declarations"
fi

case "$BUILD_ARCH" in
    i386)
        PHP_EXT_CFLAGS="$PHP_EXT_CFLAGS -m32"
        ;;
esac

cd ext
phpize
CFLAGS="$PHP_EXT_CFLAGS" ./configure
make

echo "extension=$(pwd)/modules/memprof.so"|sudo tee "$HOME/php/etc/php/memprof.ini" >/dev/null
