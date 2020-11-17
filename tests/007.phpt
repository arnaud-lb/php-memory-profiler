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
array(2) {
  ["enabled"]=>
  bool(true)
  ["native"]=>
  bool(true)
}
