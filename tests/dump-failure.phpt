--TEST--
memprof_dump_ failure
--ENV--
MEMPROF_PROFILE=1
--FILE--
<?php

require __DIR__ . '/common.php';

ini_set("error_reporting", 0);

$a = eat();
$b = Eater::eat();

$fd = fopen(__FILE__, "r");

try {
    memprof_dump_array();
} catch (\Exception $e) {
    echo "Exception: ", $e->getMessage(), "\n";
}

try {
    memprof_dump_callgrind($fd);
} catch (\Exception $e) {
    echo "Exception: ", $e->getMessage(), "\n";
}

try {
    memprof_dump_pprof($fd);
} catch (\Exception $e) {
    echo "Exception: ", $e->getMessage(), "\n";
}

--EXPECTF--
Exception: memprof_dump_callgrind(): dump failed, please check file permissions or disk capacity
Exception: memprof_dump_pprof(): dump failed, please check file permissions or disk capacity
