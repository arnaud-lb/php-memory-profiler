--TEST--
zend_pass_function crash
--ENV--
MEMPROF_PROFILE=1
--FILE--
<?php
var_dump(memprof_enabled());

class C {
}

$a = new C(1, 2, 3);

var_dump($a);

memprof_dump_array();
--EXPECT--
bool(true)
object(C)#1 (0) {
}
