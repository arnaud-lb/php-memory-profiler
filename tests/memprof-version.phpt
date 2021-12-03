--TEST--
memprof_version()
--FILE--
<?php
var_dump(memprof_version());
--EXPECTF--
string(%d) "%d.%d.%s"
