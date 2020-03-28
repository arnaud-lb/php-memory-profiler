--TEST--
memprof_dump_callgrind()
--FILE--
<?php

require __DIR__ . '/common.php';

memprof_enable();

$a = eat();
$b = Eater::eat();

memprof_dump_callgrind(STDOUT);

--EXPECT--
version: 1
cmd: unknown
positions: line
events: MemorySize BlocksCount

fl=/todo.php
fn=Eater::eat
1 8388640 1

fl=/todo.php
fn=root
1 3145760 1
cfl=/todo.php
cfn=Eater::eat
calls=1 1
1 8388640 1

total: 11534400 2

