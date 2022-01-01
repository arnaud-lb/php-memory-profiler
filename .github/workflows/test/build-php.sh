#!/bin/sh

set -ex

if which apt-get >/dev/null 2>&1; then
    sudo apt-get -y install zlib1g-dev:$BUILD_ARCH
fi

if [ "$MEMORY_CHECK" = "1" ]; then
    if which apt-get >/dev/null 2>&1; then
        sudo apt-get -y install valgrind:$BUILD_ARCH
    else
        brew install valgrind
    fi
fi

PREFIX="$HOME/php"

if ! [ -f "$HOME/build-cache/php/$PREFIX/bin/php" ]; then
    echo "PHP build is not cached"

    if [ -z "$PHP_TAG" ]; then
        wget https://secure.php.net/distributions/php-${PHP_VERSION}.tar.bz2
        tar xjf php-${PHP_VERSION}.tar.bz2
        cd php-${PHP_VERSION}
    else
        git clone --depth 1 --branch "$PHP_TAG" https://github.com/php/php-src.git
        cd php-src

        sudo apt-get -y install re2c
    fi

    if ! [ -f "configure" ]; then
        ./buildconf --force
    fi

    PHP_BUILD_FLAGS="--prefix=$PREFIX --disable-all --enable-cli --enable-cgi --with-config-file-scan-dir=$PREFIX/etc/php --with-zlib --enable-debug"

    if [ $MEMORY_CHECK -eq 1 ]; then
        PHP_BUILD_FLAGS="$PHP_BUILD_FLAGS --with-valgrind"
    fi

    case "$BUILD_ARCH" in
        i386)
            PHP_BUILD_CFLAGS="$PHP_BUILD_CFLAGS -m32"
            PHP_BUILD_ASFLAGS="$PHP_BUILD_ASFLAGS -m32"
            PHP_BUILD_FLAGS="$PHP_BUILD_FLAGS --host=$BUILD_ARCH"
            ;;
    esac

    ASFLAGS="$PHP_BUILD_ASFLAGS=" CFLAGS="$PHP_BUILD_CFLAGS" ./configure $PHP_BUILD_FLAGS
    make -j $(nproc)
    rm -rf "$HOME/build-cache/php"
    mkdir -p ~/build-cache/php
    sudo make install INSTALL_ROOT=$HOME/build-cache/php
else
    echo "PHP build is cached"
fi

sudo rsync -av "$HOME/build-cache/php$PREFIX/" "$PREFIX/"
sudo mkdir -p "$PREFIX/etc/php"
