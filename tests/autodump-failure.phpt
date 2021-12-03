--TEST--
autodump
--ENV--
MEMPROF_PROFILE=dump_on_limit
--FILE--
<?php

var_dump(memprof_enabled_flags());

$buf = str_repeat("a", 5<<20);

ini_set("memprof.output_dir", "/does-not-exist");
ini_set("memory_limit", 15<<20);

function f() {
    $a = [];
    for (;;) {
        $a[] = str_repeat("a", 1<<20);
    }
}

f();
--EXPECTF--
array(3) {
  ["enabled"]=>
  bool(true)
  ["native"]=>
  bool(false)
  ["dump_on_limit"]=>
  bool(true)
}

Fatal error: Allowed memory size of 15728640 bytes exhausted%S (tried to allocate %d bytes) (memprof failed dumping to %smemprof.callgrind%s, please check file permissions or disk capacity) in %s on line%a
