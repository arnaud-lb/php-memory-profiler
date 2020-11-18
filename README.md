# php-memprof

![Supported PHP versions: 7.1 ... 8.x](https://img.shields.io/badge/php-7.1%20...%208.x-blue.svg) [![Build Status](https://travis-ci.com/arnaud-lb/php-memory-profiler.svg?branch=v2)](https://travis-ci.com/arnaud-lb/php-memory-profiler)

php-memprof is a fast and accurate memory profiling extension for PHP that can be used to detect memory leaks.

## Features

The extension tracks the allocation and release of memory blocks to report the amount of memory leaked by every function, method, or file in a program.

 * Reports non-freed memory at arbitrary points in the program
 * Dumps profile in callgrind, pprof, or raw array formats
 * Can track memory allocated by PHP itself as well as native malloc

### How does it differ from Blackfire ?

Accuracy: Blackfire tracks how functions affect the total memory usage (memory_usage_after - memory_usage_before, for each function call). Memprof tracks every allocated memory block in the program, and reports what has not been freed at an arbitrary point in the program. In effect, the difference between these two strategies is that functions that allocate small amounts of memory but whose allocations are retained may not be visible in Blackfire, whereas functions that allocate big chunks of memory that are freed by the caller will look like big memory consumers although they are not really.

Native malloc: Memprof can track the memory allocated with malloc(), in addition to PHP's own allocator.

## Install

### Dependencies

php-memprof depends on libjudy. On Debian or Ubuntu, the dependency can be
installed with:

    # install libjudy dependency:
    apt install libjudy-dev

### Using PECL

Make sure to install dependencies, and then:

    pecl install memprof

### Manually

Make sure to install dependencies, and then:

Download the source and run the following commands in the source directory:

    phpize
    ./configure
    make
    make install

## Loading the extension

The extension can be loaded on the command line, just for one script:

    php -dextension=memprof.so script.php

Or permanently, in php.ini:

    extension=memprof.so

The extension has no overhead when not profiling, so it can be loaded by default on dev environments.

## Usage

Using the extension is done in three steps:

### 1. Enabling profile

Profiling is enabled at request startup when one of these is true:

 * The environment variable `MEMPROF_PROFILE` is non-empty
 * `$_GET["MEMPROF_PROFILE"]` is non-empty
 * `$_POST["MEMPROF_PROFILE"]` is non-empty

### 2. Dumping the profile

Once profiling is enabled, the program must call ``memprof_dump_callgrind()`` or
one it its variants to dump the memory profile.

This can be done at anytime during the program, ideally when the leak is large,
so that it will be more visible in the profile.

This can be done multiple times during the same execution, if necessary.

### 3. Visualizing the profile

The profile can be visualised with Kcachegrind, Qcachegrind, Google Perftools,
or with custom tools. See the documentation of ``memprof_dump_callgrind()``
and variants.

### Usage example

```
<?php // test.php

do_some_work();

if (function_exists('memprof_enabled') && memprof_enabled()) {
    memprof_dump_callgrind(fopen("/tmp/callgrind.out", "w"));
}
```

When ran on the command line, profiling can be enabled by setting the `MEMPROF_PROFILE` environment variable:

```
MEMPROF_PROFILE=1 php test.php
```

When ran in a web context, profiling can be enabled by setting the `MEMPROF_PROFILE` query string parameter or POST field:

```
curl http://127.0.0.1/test.php?MEMPROF_PROFILE=1
```

Setting a POST field works as well:

```
curl -d MEMPROF_PROFILE=1 http://127.0.0.1/test.php
```

## Functions documentation

### memprof_enabled()

Returns whether memory profiling is currently enabled (see above).

### memprof_dump_callgrind(resource $stream)

Dumps the current profile in callgrind format. The result can be visualized with tools such as
[KCacheGrind][2] or [QCacheGrind][6].

``` php
<?php
memprof_dump_callgrind(fopen("output", "w"));
```

Here is a QcacheGrind screenshot:

![QCacheGrind screenshot](https://raw.githubusercontent.com/arnaud-lb/php-memory-profiler/v2/assets/qcachegrind.png)

### memprof_dump_pprof(resource $stream)

Dumps the current profile in [pprof][4] format.

``` php
<?php
memprof_dump_pprof(fopen("profile.heap", "w"));
```

The file can be visualized with [google-perftools][5]'s [``pprof``][4] tool.

Display annotated call-graph in web browser or in ``gv``:

```
$ pprof --web profile.heap
$ # or:
$ pprof --gv profile.heap
```

![pprof call-graph screenshot](https://i.stack.imgur.com/EAnGC.png)

Output one line per function, sorted by own memory usage:

```
$ pprof --text profile.heap
```

### memprof_dump_array()

Returns an array representing the current profile.

``` php
<?php
$dump = memprof_dump_array();
```

The array exposes the following information:

 * Inclusive and exclusive memory leaked by functions (counting only the memory
   that has is still not freed when memprof_dump_array is called)
 * Inclusive and exclusive blocks count of functions (number of allocations;
   counting only the blocks that are still not freed when memprof_dump_array is
   called)
 * The data is presented in call stacks. This way, if a function is called from
   multiple places, it is possible to see which call path caused it to leak the
   most memory

Example output:

    Array
    (
      [memory_size] => 11578
      [blocks_count] => 236
      [memory_size_inclusive] => 10497691
      [blocks_count_inclusive] => 244
      [calls] => 1
      [called_functions] => Array
        (
          [main] => Array
            (
              [memory_size] => 288
              [blocks_count] => 3
              [memory_size_inclusive] => 10486113
              [blocks_count_inclusive] => 8
              [calls] => 1
              [called_functions] => Array
                (
                  [a] => Array
                    (
                      [memory_size] => 4
                      [blocks_count] => 1
                      [memory_size_inclusive] => 10485825
                      [blocks_count_inclusive] => 5
                      [calls] => 1
                      [called_functions] => Array
                        (
                          [b] => Array
                            (
                              [memory_size] => 10485821
                              [blocks_count] => 4
                              [memory_size_inclusive] => 10485821
                              [blocks_count_inclusive] => 4
                              [calls] => 1
                              [called_functions] => Array
                                (
                                  [str_repeat] => Array
                                    (
                                      [memory_size] => 0
                                      [blocks_count] => 0
                                      [memory_size_inclusive] => 0
                                      [blocks_count_inclusive] => 0
                                      [calls] => 1
                                      [called_functions] => Array
                                        (
                                        )
                                    )
                                )
                            )
                        )
                    )
                  [memprof_dump_array] => Array
                    (
                      [memory_size] => 0
                      [blocks_count] => 0
                      [memory_size_inclusive] => 0
                      [blocks_count_inclusive] => 0
                      [calls] => 1
                      [called_functions] => Array
                        (
                        )
                    )
                )
            )
        )
    )

## Troubleshooting

 * If you are experiencing crashes, try disabling malloc hooks by setting HAVE_MALLOC_HOOKS to 0 in config.h after running configure; then run ``make clean && make && make install``. (Using malloc hooks may crash if some other extension uses threads internally.)
 * The extensions may conflict with xdebug, blackfire, or other extensions. If that's the case for you, please report it.

## PHP versions

The current branch supports PHP 7 and PHP 8.

The php5 branch supports PHP 5.

## TODO

 * Thread-safe malloc hooks

## How it works

See [INTERNALS.md][7]

[1]: https://www.gnu.org/software/libc/manual/html_node/Hooks-for-Malloc.html#Hooks-for-Malloc
[2]: https://kcachegrind.github.io/html/Home.html
[3]: http://judy.sourceforge.net/index.html
[4]: https://github.com/google/pprof
[5]: https://github.com/gperftools/gperftools
[6]: https://www.google.com/search?q=qcachegrind
[7]: https://github.com/arnaud-lb/php-memory-profiler/blob/master/INTERNALS.md
