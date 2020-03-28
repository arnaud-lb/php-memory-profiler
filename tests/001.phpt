--TEST--
Enable/disable
--FILE--
<?php

var_dump(memprof_enabled());

memprof_enable();

var_dump(memprof_enabled());

memprof_disable();

var_dump(memprof_enabled());

--EXPECT--
bool(false)
bool(true)
bool(false)

