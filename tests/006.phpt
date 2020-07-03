--TEST--
Enable with post field
--POST--
MEMPROF_PROFILE=1
--FILE--
<?php
var_dump(memprof_enabled());
memprof_dump_array();
--EXPECT--
bool(true)
