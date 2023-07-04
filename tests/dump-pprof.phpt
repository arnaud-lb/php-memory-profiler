--TEST--
memprof_dump_pprof()
--ENV--
MEMPROF_PROFILE=1
--FILE--
<?php

require __DIR__ . '/common.php';

$a = eat();
$b = Eater::eat();

memprof_dump_pprof(STDOUT);

--EXPECTF--
--- symbol
binary=php://stdout
0x0000000000000008 root
0x0000000000000010 require %sdump-pprof.php
0x0000000000000018 require %scommon.php
0x0000000000000020 eat
0x0000000000000028 str_repeat
0x0000000000000030 Eater::eat
0x0000000000000038 memprof_dump_pprof
---
--- profile
%s
