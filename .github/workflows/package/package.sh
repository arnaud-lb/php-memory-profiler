#!/bin/sh

set -e

cd ext

echo "Checking version consistency"

CODE_VERSION="$(grep PHP_MEMPROF_VERSION php_memprof.h|cut -d'"' -f2)"
PACKAGE_VERSION="$(grep -m 1 '<release>' package.xml|cut -d'>' -f2|cut -d'<' -f1)"

if ! [ "$CODE_VERSION" = "$PACKAGE_VERSION" ]; then
    printf "Version in php_memprof.h does not match version in package.xml: '%s' vs '%s'" "$CODE_VERSION" "$PACKAGE_VERSION" >&2
    exit 1
fi

echo "Packaging"

pecl package

echo "Installing package.xml"

mv "./memprof-$PACKAGE_VERSION.tgz" memprof.tgz
sudo pecl install ./memprof.tgz

echo "Checking that all test files was included"

sudo pecl list-files memprof|grep ^test|sed 's@.*/tests/@@'|sort > installed-test-files
find tests/ -type f|sed 's@^tests/@@'|sort > repository-test-files

if ! diff -u repository-test-files installed-test-files; then
    echo "Some test files are missing from package.xml (see diff above)" >&2
    exit 1
fi
