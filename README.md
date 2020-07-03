# php-memprof

![Supported PHP versions: 7.0 ... 8.x](https://img.shields.io/badge/php-7.0%20...%208.x-blue.svg)

php-memprof is a memory profiler for PHP that can be used to detect memory leaks.

## Install

### Dependencies

php-memprof depends on libjudy. On Debian or Ubuntu, the dependency can be
installed with:

    # install libjudy dependency:
    apt instal libjudy-dev

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

## Usage

Profiling is enabled at request startup when one of these is true:

 * The environment variable `MEMPROF_PROFILE` is non-empty
 * `$_GET["MEMPROF_PROFILE"]` is non-empty
 * `$_POST["MEMPROF_PROFILE"]` is non-empty

Once profiling is enabled, the program must call ``memprof_dump_callgrind`` or
one it its variants".

Example:

```
<?php // test.php

do_some_work();

if (function_exists('memprof_enabled') && memprof_enabled()) {
    memprof_dump_callgrind(fopen("/tmp/callgrind.out", "w"));
}
```

```
MEMPROF_PROFILE=1 php test.php
```

Or:

```
curl http://127.0.0.1/test.php?MEMPROF_PROFILE=1
```

Whem using ``memprof_dump_callgrind``, the profile can be visualized with
Kcachegrind or Qcachegrind (see bellow).

### memprof_enabled()

Returns whether memprof is enabled.

### memprof_dump_callgrind(resource $stream)

The memprof_dump_callgrind function dumps the current profile to a stream
in callgrind format. The file can then be read with tools such as
[KCacheGrind][2] or [QCacheGrind][6].

``` php
<?php
memprof_dump_callgrind(fopen("output", "w"));
```

Here is a QcacheGrind screenshot:

![QCacheGrind screenshot](https://raw.githubusercontent.com/arnaud-lb/php-memory-profiler/master/assets/qcachegrind.png)

### memprof_dump_pprof(resource $stream)

The memprof_dump_pprof function dumps the current memory usage to a stream in
[pprof][4] format.

``` php
<?php
memprof_dump_pprof(fopen("profile.heap", "w"));
```

The file can be visualized using [google-perftools][5]'s [``pprof``][4] tool.

Display annotated call-graph in web browser or in ``gv``:

```
$ pprof --web profile.heap
$ # or:
$ pprof --gv profile.heap
```

![pprof call-graph screenshot](http://img707.imageshack.us/img707/7697/screenshot3go.png)

Output one line per function, sorted by own memory usage:

```
$ pprof --text profile.heap
```

### memprof_dump_array()

``` php
<?php
$dump = memprof_dump_array();
```

The dump exposes the following information:

 * Inclusive and exclusive memory usage of functions (counting only the memory
   that has is still in use when memprof_dump_array is called)
 * Inclusive and exclusive blocks count of functions (number of allocated;
   counting only the blocks that are still in use when memprof_dump_array is
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

## PHP 7, PHP 8

The current branch supports PHP 7 and PHP 8.

## PHP 5

The php5 branch supports PHP 5.

## How it works

See [INTERNALS.md][7]

[1]: https://www.gnu.org/software/libc/manual/html_node/Hooks-for-Malloc.html#Hooks-for-Malloc
[2]: https://kcachegrind.github.io/html/Home.html
[3]: http://judy.sourceforge.net/index.html
[4]: https://github.com/google/pprof
[5]: https://github.com/gperftools/gperftools
[6]: https://www.google.com/search?q=qcachegrind
[7]: https://github.com/arnaud-lb/php-memory-profiler/blob/master/INTERNALS.md
