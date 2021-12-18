--TEST--
autodump vs xdebug
--EXTENSIONS--
xdebug
--INI--
xdebug.mode=develop
--ENV--
MEMPROF_PROFILE=dump_on_limit
--FILE--
<?php

$dir = sys_get_temp_dir() . '/' . microtime(true);
var_dump($dir);
var_dump(mkdir($dir));
var_dump(memprof_enabled_flags());

$buf = str_repeat("a", 5<<20);

register_shutdown_function(function () use (&$buf, $dir) {
    $buf = "";
    var_dump(scandir($dir));
});

ini_set("memprof.output_dir", $dir);
ini_set("memory_limit", 15<<20);

function f() {
    $a = [];
    for (;;) {
        $a[] = str_repeat("a", 1<<20);
    }
}

f();
--EXPECTF--
%sautodump-xdebug.php:%d:
string(%d) "/%s"
%sautodump-xdebug.php:%d:
bool(true)
%sautodump-xdebug.php:%d:
array(3) {
  'enabled' =>
  bool(true)
  'native' =>
  bool(false)
  'dump_on_limit' =>
  bool(true)
}

Fatal error: Allowed memory size of 15728640 bytes exhausted%S (tried to allocate %d bytes) (memprof dumped to %smemprof.callgrind%s) in %s on line%a

Call Stack:
    %s
    %s
    %s

%sautodump-xdebug.php:%d:
array(3) {
  [0] =>
  string(1) "."
  [1] =>
  string(2) ".."
  [2] =>
  string(%d) "memprof.callgrind.%s"
}
