--TEST--
memprof_dump_array()
--FILE--
<?php

require __DIR__ . '/common.php';

try {
	memprof_dump_array();
} catch ( Throwable $ex ) {
	var_dump(get_class($ex));
}

$ignoredAlloc = eat();

memprof_enable();

var_dump(memprof_dump_array());

$a = eat();
$b = Eater::eat();

var_dump(memprof_dump_array());

--EXPECTF--
string(9) "Exception"

Warning: Calling memprof_enable() manually may not work as expected because of PHP optimizations. Prefer using MEMPROF_PROFILE=1 as environment variable, GET, or POST in %s
array(6) {
  ["memory_size"]=>
  int(0)
  ["blocks_count"]=>
  int(0)
  ["memory_size_inclusive"]=>
  int(0)
  ["blocks_count_inclusive"]=>
  int(0)
  ["calls"]=>
  int(1)
  ["called_functions"]=>
  array(0) {
  }
}
array(6) {
  ["memory_size"]=>
  int(3145760)
  ["blocks_count"]=>
  int(1)
  ["memory_size_inclusive"]=>
  int(11534400)
  ["blocks_count_inclusive"]=>
  int(2)
  ["calls"]=>
  int(1)
  ["called_functions"]=>
  array(1) {
    ["Eater::eat"]=>
    array(6) {
      ["memory_size"]=>
      int(8388640)
      ["blocks_count"]=>
      int(1)
      ["memory_size_inclusive"]=>
      int(8388640)
      ["blocks_count_inclusive"]=>
      int(1)
      ["calls"]=>
      int(1)
      ["called_functions"]=>
      array(0) {
      }
    }
  }
}
