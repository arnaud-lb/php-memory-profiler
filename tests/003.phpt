--TEST--
memprof_dump_callgrind()
--FILE--
<?php

require __DIR__ . '/common.php';

memprof_enable();

$a = eat();
$b = Eater::eat();

memprof_dump_callgrind(STDOUT);

--EXPECTF--
Warning: Calling memprof_enable() manually may not work as expected because of PHP optimizations. Prefer using MEMPROF_PROFILE=1 as environment variable, GET, or POST in %s
version: 1
cmd: unknown
positions: line
events: MemorySize BlocksCount

fl=%s/tests/common.php
fn=Eater::eat
1 8388640 1

fl=
fn=root
1 3145760 1
cfl=%s/tests/common.php
cfn=Eater::eat
calls=1 1
1 8388640 1

total: 11534400 2

