# php-memprof

php-memprof profiles memory usage of PHP scripts, and especially can tell which
function has allocated every single byte of memory currently allocated.

This is different from measuring the memory usage before and after a
function call:

``` php
<?php

// script 1

function a() {
    $data = file_get_contents("huge-file");
}

a();

$profile = memprof_dump_array();

```

In script 1, a before/after approach would designate file_get_contents() as huge
memory consumer, while the memory it allocates is actually freed quickly after
it returns. When dumping the memory usage after a() returns, the memprof
approach would show that file_get_contents() is a small memory consumer since
the memory it allocated has been freed at the time a() has returned.


``` php
<?php

// script 2

function a() {
    global $cache;
    $cache = file_get_contents("huge-file");
}

a();

$profile = memprof_dump_array();
```

In script 2, the allocated memory remains allocated after file_get_contents()
and a() return. This time a() and file_get_contents() are shown as huge memory
consumers.

## Install

    phpize
    ./configure
    make
    make install

## Dependencies

 * C Library with [malloc hooks][1]

## Loading the extension

The extension must be loaded in a special way to make it work properly:

 * it must be preloaded with LD_PRELOAD or equivalent (compiling it statically
   in PHP, or linking PHP to the extension may work too).
 * it must also be loaded as a zend_extension

Example:

    LD_PRELOAD=/absolute/path/to/memprof.so php -dzend_extension=/absolute/path/to/memprof.so script.php

## Usage

The memory usage can be dumped by calling the memprof_dump_array() or
memprof_dump_callgrind() functions.

### memprof_dump_callgrind

The memprof_dump_callgrind function dumps the current memory usage to a stream
in callgrind format. The file can then be read with tools such as
[KCacheGrind][2] or [WebGrind][3]. Xdebug lists other tools capable of reading
callgrind files: http://xdebug.org/docs/profiler

``` php
<?php
memprof_dump_callgrind(fopen("output", "w"));
?>
```

This is probably the best way to visualize memory usage.

### memprof_dump_array

``` php
<?php
$dump = memprof_dump_array();
?>
```

The dump exposes the following informations:

 * Inclusive and exlusive memory usage of functions (counting only the memory
   that has is still in use when memprof_dump_array is called)
 * Inclusive and exlusive blocks count of functions (number of allocated;
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

## Todo

 * Only tested in command line; may not work in CGI or server modules
 * Only tested on Linux boxes; may not compile elsewhere
 * Make it work without LD_PRELOAD: the extension currently takes profit of the
   fact that it sees every single malloc() since the existance of the process,
   which simplies handling of free and realloc. Hooking later requires to be
   able to recognize if blocks were allocated by the extension in free and
   realloc.
 * Make it work without disabling the Zend memory allocator; or implement
   memory_limit and memory_get_usage()

[1]: https://www.gnu.org/software/libc/manual/html_node/Hooks-for-Malloc.html#Hooks-for-Malloc
[2]: http://kcachegrind.sourceforge.net/html/Home.html
[3]: http://xdebug.org/docs/profiler

