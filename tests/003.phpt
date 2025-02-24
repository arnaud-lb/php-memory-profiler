--TEST--
memprof_dump_callgrind()
--ENV--
MEMPROF_PROFILE=1
--FILE--
<?php

require __DIR__ . '/common.php';

$a = eat();
$b = Eater::eat();

memprof_dump_callgrind(STDOUT);

--EXPECTF--
version: 1
cmd: unknown
positions: line
events: Memory_Size_(bytes) BlocksCount

fl=%scommon.php
fn=require %scommon.php
1 0 0

fl=php:internal
fn=str_repeat
1 %d 1

fl=%scommon.php
fn=eat
1 0 0
cfl=php:internal
cfn=str_repeat
calls=1 1
1 %d 1

fl=php:internal
fn=str_repeat
1 0 0

fl=%scommon.php
fn=eat
1 0 0
cfl=php:internal
cfn=str_repeat
calls=1 1
1 0 0

fl=php:internal
fn=str_repeat
1 0 0

fl=%scommon.php
fn=Eater::eat
1 %d 1
cfl=%scommon.php
cfn=eat
calls=1 1
1 0 0
cfl=php:internal
cfn=str_repeat
calls=1 1
1 0 0

fl=php:internal
fn=memprof_dump_callgrind
1 0 0

fl=%s003.php
fn=require %s003.php
1 %d 10
cfl=%scommon.php
cfn=require %scommon.php
calls=1 1
1 0 0
cfl=%scommon.php
cfn=eat
calls=1 1
1 %d 1
cfl=%scommon.php
cfn=Eater::eat
calls=1 1
1 %d 1
cfl=php:internal
cfn=memprof_dump_callgrind
calls=1 1
1 0 0

fl=
fn=root
1 %d %d
cfl=%s003.php
cfn=require %s003.php
calls=1 1
1 %d 12

total: %d %d
