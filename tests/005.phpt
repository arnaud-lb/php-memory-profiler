--TEST--
Enable with query string parameter
--GET--
MEMPROF_PROFILE=1
--FILE--
<?php
var_dump(memprof_enabled());
memprof_dump_array();
--EXPECT--
bool(true)
