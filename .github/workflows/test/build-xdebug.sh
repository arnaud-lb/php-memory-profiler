#!/bin/sh

set -e

export PATH="$HOME/php/bin:$PATH"
hash -r

git clone --depth 1 --branch "3.1.2" "https://github.com/xdebug/xdebug.git"

cd xdebug

phpize
./configure
make
sudo make install
