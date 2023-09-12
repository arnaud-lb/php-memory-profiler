#!/bin/sh

set -ex

case "$BUILD_ARCH" in
    i386)
        echo "Setup $BUILD_ARCH architecture"
        sudo dpkg --add-architecture "$BUILD_ARCH"
        sudo apt-get update
        sudo apt-get install gcc-multilib
        ;;
esac
