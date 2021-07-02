--TEST--
Enable native profiling
--SKIPIF--
<?php getenv('MEMPROF_EXPECT_NATIVE_TRACKING') === '1' || die("skip no native tracking");
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
