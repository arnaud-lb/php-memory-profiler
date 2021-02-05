--TEST--
Enable native profiling
--ENV--
MEMPROF_PROFILE=native
--FILE--
<?php
var_dump(memprof_enabled());
var_dump(memprof_enabled_flags());
--EXPECT--
bool(true)
array(3) {
  ["enabled"]=>
  bool(true)
  ["native"]=>
  bool(true)
  ["dump_on_limit"]=>
  bool(false)
}
